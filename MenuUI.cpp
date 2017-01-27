/* 
   This file is part of MenuUI, a flexible menu driven user interface 
   for Arduino.
   Copyright (C) 2016-2017 Daniel Pena <trifling.github@gmail.com>

   MenuUI is free software: you can redistribute it and/or modify it under the
   terms of the GNU General Public License as published by the Free Software
   Foundation, either version 3 of the License, or (at your option) any later
   version.

   MenuUI is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along with
   closest. If not, see <http://www.gnu.org/licenses/>.
*/


#include "MenuUI.h"

/*
 * AUX FUNCS
 */
static void strclr( char *dst, uint8_t size ) {
   memset( dst, ' ', size-2 ); 
   dst[size-1] = '\0';
}

static void strpad( char *dst, uint8_t siz ) {
   register char *d = dst;
   register uint8_t n = siz;
   
   if(n != 0 && --n != 0) {
      do {
         if( *d++ == 0 )
            break;
      } while (--n != 0);
   }

   if(n != 0) {
      *(d-1) = ' ';
      do {
         *d++ = ' ';
      } while( --n != 0 );
      *d = '\0';
   }
}

static uint8_t strftime( uint8_t d, uint8_t h, uint8_t m, uint8_t s, char *out, uint8_t n ) {
   char tmp[12];
   memset( out, 0, n );
   itoa(d,tmp,10); strcat( out, tmp ); strcat( out, ":" );
   itoa(h,tmp,10); strcat( out, tmp ); strcat( out, ":" ); 
   itoa(m,tmp,10); strcat( out, tmp ); strcat( out, ":" );
   itoa(s,tmp,10); strcat( out, tmp );
   return strlen(out);
}

/*
 *
 * OPTIONS
 *
 */
template<>
void NumericOption<int>::asString( int val, char *s, uint8_t n ) {
   itoa( val, s, 10 );
   if( m_unit != NULL )
      strcat( s, m_unit );
}

template<>
void NumericOption<float>::asString( float val, char *s, uint8_t n ) {
   dtostrf( val, 2*n/3, n/3, s );
   if( m_unit != NULL )
      strcat( s, m_unit );
}

template<>
void NumericOption<double>::asString( double val, char *s, uint8_t n ) {
   dtostrf( val, 2*n/3, n/3, s );
}

void MenuUI::setBlinkTime( uint16_t msecs ) {
   m_blink_msecs = msecs;
}

void MenuUI::setLCDOffTime( uint16_t msecs ) {
   m_lcdoff_msecs = msecs;
}

void MenuUI::setValueUpdateTime( uint16_t msecs ) {
   m_value_msecs = msecs;
}

MenuUI::MenuUI() {
   m_lcdon        = 1;
   m_detail       = 0;
   m_current      = MENU_UNDEF;
   m_last_time    = 0;
   m_blink_time   = 0;
   m_blink_status = 0;
   m_value_time   = 0;

   m_blink_msecs  = MENU_BLINK_MSECS;
   m_lcdoff_msecs = MENU_LCDOFF_MSECS;
   m_value_msecs  = MENU_VALUE_MSECS;
   m_line_size    = MENU_LINE_SIZE;

   m_highmillis = 0;
   m_rollovers  = 0;
   m_line1 = NULL;
   m_line2 = NULL;
}

MenuUI::~MenuUI() {
   if( m_line1 != NULL )
      delete m_line1;
   if( m_line2 != NULL )
      delete m_line2;
}

void MenuUI::setup( MenuItem *items, uint8_t line_size, lcd_fun lfun, val_fun vfun ) {
   m_line_size = line_size + 1; // for the \0
   m_vfun = vfun;
   m_lfun = lfun;
   m_line1 = new char[m_line_size];
   m_line2 = new char[m_line_size];

   //m_origs = items;
   //uint8_t m_n_items = 0;
   //while( pgm_read_byte( &(m_origs[m_n_items].type) ) != MENU_END )
      //m_n_items++;
   //m_n_items++;

   m_items = items;
   m_items[0].parent = MENU_UNDEF;
   m_items[0].count  = 0;
   m_items[0].first  = 1;

   uint8_t parents[10] = {MENU_UNDEF};
   uint8_t nparent = 1;
   uint8_t item = 1;
   uint8_t m_n_items = 0;
   while(1) {
      
      uint8_t parent = parents[nparent];
      
      m_items[item].parent = parent;
      m_items[parent].count++;

      if( m_items[item].type == MENU_SUBMENU ) {
         nparent++; 
         parents[nparent] = item;
         uint8_t k = 1;
         while(true) {
            if( m_items[k].type == MENU_END ) 
               break;
            if( m_items[item].sub_id == m_items[k].id ) {
               m_items[item].first = k;
               break;
            }
            k++;
         }
         item = m_items[item].first;
         if( item > m_n_items )
            m_n_items = item+1;

      } else if( m_items[item].type == MENU_EXIT ) {
         break;

      } else if( m_items[item].type == MENU_BACK ) {
         item = parents[nparent] + 1; 
         nparent--;

      } else {
         item++;
         if( item > m_n_items )
            m_n_items = item+1;
      }
   }
}

enum {
   NOTHING = 0,
   LINE1 = 1,
   LINE2 = 2,
   LINES = 3,
};
 
// strcpy from flash mem
char *strcpy_ff( char *dst, const char *src ) {
   if( src != NULL ) 
      strcpy_P( dst, src );
   else
      dst[0] = '\0';
}

void MenuUI::update( unsigned long current_time, int8_t delta, int8_t button  ) {
 
   if( current_time > 3*UINT64_MAX/4 )
      m_highmillis = 1;
   if( current_time <= 100000 && m_highmillis ) {
      m_rollovers++;
      m_highmillis = 0;
   }

   if( button != 0 ) {
      Serial.print("button pressed: "); Serial.println(button);
   }

   int16_t current = m_current;
   uint8_t detail = m_detail;
   uint8_t lines = 0;
   uint8_t parent = 0;

   // handle input
   if( button == 2 ) {
      goto exit_and_off;

   } else if( button == 1 ) {

      lines |= LINES;
      if( m_items[m_current].type == MENU_EXIT ) {
         goto exit_and_off;

      } else if( m_current == MENU_UNDEF ) {
         current = 1;

      } else if( m_items[m_current].type == MENU_SUBMENU ) {
         current = m_items[m_current].first;

      } else if( m_items[m_current].type == MENU_BACK ) {
         current = m_items[m_current].parent;

      } else {
         if( m_detail ) { 
            detail = 0;
            if( m_items[m_current].type == MENU_OPTION ) 
               m_items[m_current].option->endChange();
         } else {
            detail = 1;
            if( m_items[m_current].type == MENU_OPTION ) 
               m_items[m_current].option->startChange();
         }
      }

   } else if( delta != 0 ) {

      if( m_current == MENU_UNDEF ) {
         current = 1;
         lines |= LINES;

      } else if( m_detail ) {
         if( m_items[m_current].type & MENU_OPTION ) {
            m_items[m_current].option->change(delta);
            lines |= LINE2;
         }
      } else {

         uint8_t parent = m_items[m_current].parent;
         int16_t offset = 0, length = 1;
         if( parent != MENU_UNDEF ) {
            offset = m_items[parent].first;
            length = m_items[parent].count;
         } 

         current = m_current + delta;
         if( current >= offset + length )
            current = offset + (current-offset)%length;

         else if( current < offset ) {
            int16_t mod = (current-offset)%length;
            if( mod < 0 )
               mod += length;
            current = offset + mod;
         }
         lines |= LINE2;
      }
   }

   // select current
   if( current == m_current && detail == m_detail ) {
      lines = 0;
   }
   
   // blinking
   if( m_items[current].type == MENU_OPTION && current_time-m_blink_time > m_blink_msecs && m_detail ) {
      m_blink_status ^= 1;
      m_blink_time = current_time;
      lines |= LINE2;
   }
   
   // variable data sources
   if( m_items[current].type == MENU_VALUE && current_time-m_value_time > m_value_msecs && m_detail ) {
      m_value_time = current_time;
      lines |= LINE2;
   }

   // check for timeout
   if( current_time-m_last_time > m_lcdoff_msecs && m_lcdon && ( current==MENU_UNDEF || !lines ) ) 
      goto exit_and_off;

   // update timeout counter
   if( !lines ) 
      goto exit;

   if( !m_lcdon || m_items[m_current].type == MENU_BACK )
      lines |= LINES;

   m_last_time = current_time;

   // update the screen if needed 
   parent = m_items[current].parent;
   if( m_items[current].type == MENU_OPTION && detail ) {
      
      if( lines & LINE1 ) {
         strcpy_ff( m_line1, m_items[current].caption ); 
         strpad( m_line1, m_line_size );
      }

      if( lines & LINE2 && m_blink_status ) {
         m_items[current].option->asString(m_line2,m_line_size);
         strpad( m_line2, m_line_size );
      } else {
         strclr( m_line2, m_line_size );
      }

   } else if( m_items[current].type == MENU_VALUE && detail ) {

      if( lines & LINE1 ) {
         strcpy_ff( m_line1, m_items[current].caption );
         strpad( m_line1, m_line_size );
      }
         
      if( lines & LINE2 ) {
         (*m_vfun)( m_items[current].id, m_line2 ); 
         strpad( m_line2, m_line_size );
      }

   } else if( m_items[current].type == MENU_BACK ) {
      strcpy_ff( m_line1, m_items[current].caption );
      strcpy_ff( m_line2, m_items[m_items[parent].parent].caption );
      strpad( m_line1, m_line_size );
      strpad( m_line2, m_line_size );
      lines |= LINES;

   } else {
 
      if( lines & LINE1 ) {
         strcpy_ff( m_line1, m_items[parent].caption );
         strpad( m_line1, m_line_size );
      }
      if( lines & LINE2) {
         strcpy_ff( m_line2, m_items[current].caption );
         strpad( m_line2, m_line_size );
      }
   }


   if( lines & LINE1 )
      m_lfun( MENU_LCD_PRINT_LINE1, m_line1 );
   if( lines & LINE2 )
      m_lfun( MENU_LCD_PRINT_LINE2, m_line2 );

   if( !m_lcdon ) {
      m_lfun( MENU_LCD_ON, NULL );
      m_lcdon = 1;
   }

exit:
   m_current = current;
   m_detail = detail;
   return; 

exit_and_off:
   m_detail = 0;
   m_lcdon  = 0;
   m_current = MENU_UNDEF;
   strclr( m_line1, m_line_size );
   strclr( m_line2, m_line_size );
   m_lfun( MENU_LCD_PRINT_LINE1, m_line1 ); 
   m_lfun( MENU_LCD_PRINT_LINE2, m_line2 );
   m_lfun( MENU_LCD_OFF, NULL );
   return;
}

void MenuUI::uptime( char *out ) {
   uint8_t d, h, m, s;
   unsigned long up = millis()/1000;
   s = up%60;
   m = (up/60)%60;
   h = (up/60/60)%24;
   d = m_rollovers*UINT64_MAX/1000/60/60/24 + up/60/60/24;
   strftime( d, h, m, s, out, m_line_size ); 
}

