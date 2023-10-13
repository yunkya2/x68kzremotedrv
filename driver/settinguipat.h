/*
 * Copyright (c) 2023 Yuichi Nakamura (@yunkya2)
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>
#include <x68k/iocs.h>

#ifndef _SETTINGUIPAT_H_
#define _SETTINGUIPAT_H_

const struct iocs_fntbuf keybdpat[] =
{
  /*    #a
  --------++++++++
   ############## 
  #######  #######
  ###### ## ######
  ##### #### #####
  #### ###### ####
  ### ######## ###
  ##    ####    ##
  ##### #### #####
  ##### #### #####
  ##### #### #####
  ##### #### #####
  ##### #### #####
  ##### #### #####
  ##### #### #####
  #####      #####
   ############## 
  */
  { 16, 16,
    0x7f, 0xfe, 0xfe, 0x7f, 0xfd, 0xbf, 0xfb, 0xdf,
    0xf7, 0xef, 0xef, 0xf7, 0xc3, 0xc3, 0xfb, 0xdf,
    0xfb, 0xdf, 0xfb, 0xdf, 0xfb, 0xdf, 0xfb, 0xdf,
    0xfb, 0xdf, 0xfb, 0xdf, 0xf8, 0x1f, 0x7f, 0xfe,
  },
  /*    #b
  --------++++++++
   ############## 
  #####      #####
  ##### #### #####
  ##### #### #####
  ##### #### #####
  ##### #### #####
  ##### #### #####
  ##### #### #####
  ##### #### #####
  ##    ####    ##
  ### ######## ###
  #### ###### ####
  ##### #### #####
  ###### ## ######
  #######  #######
   ############## 
  */
  { 16, 16,
    0x7f, 0xfe, 0xf8, 0x1f, 0xfb, 0xdf, 0xfb, 0xdf,
    0xfb, 0xdf, 0xfb, 0xdf, 0xfb, 0xdf, 0xfb, 0xdf,
    0xfb, 0xdf, 0xc3, 0xc3, 0xef, 0xf7, 0xf7, 0xef,
    0xfb, 0xdf, 0xfd, 0xbf, 0xfe, 0x7f, 0x7f, 0xfe, 
  },
  /*    #c
  --------++++++++
   ############## 
  ################
  ###### #########
  #####  #########
  #### # #########
  ### ##         #
  ## ########### #
  # ############ #
  # ############ #
  ## ########### #
  ### ##         #
  #### # #########
  #####  #########
  ###### #########
  ################
   ############## 
  */
  { 16, 16,
    0x7f, 0xfe, 0xff, 0xff, 0xfd, 0xff, 0xf9, 0xff,
    0xf5, 0xff, 0xec, 0x01, 0xdf, 0xfd, 0xbf, 0xfd,
    0xbf, 0xfd, 0xdf, 0xfd, 0xec, 0x01, 0xf5, 0xff,
    0xf9, 0xff, 0xfd, 0xff, 0xff, 0xff, 0x7f, 0xfe,
  },
  /*    #d
  --------++++++++
   ############## 
  ################
  ######### ######
  #########  #####
  ######### # ####
  #         ## ###
  # ########### ##
  # ############ #
  # ############ #
  # ########### ##
  #         ## ###
  ######### # ####
  #########  #####
  ######### ######
  ################
   ############## 
  */
  { 16, 16,
    0x7f, 0xfe, 0xff, 0xff, 0xff, 0xbf, 0xff, 0x9f,
    0xff, 0xaf, 0x80, 0x37, 0xbf, 0xfb, 0xbf, 0xfd,
    0xbf, 0xfd, 0xbf, 0xfb, 0x80, 0x37, 0xff, 0xaf,
    0xff, 0x9f, 0xff, 0xbf, 0xff, 0xff, 0x7f, 0xfe,
  },
  /*    #e
  --------++++++++--------
   ###################### 
  ################      ##
  ################ #### ##
  ################ #### ##
  ################ #### ##
  ##### ########## #### ##
  ####  ########## #### ##
  ### #           ##### ##
  ## ################## ##
  # ################### ##
  # ################### ##
  ## ################## ##
  ### #                ###
  ####  ##################
  ##### ##################
   ###################### 
  */
  { 24, 16,
    0x7f, 0xff, 0xfe, 0xff, 0xff, 0x03, 0xff, 0xff, 0x7b, 0xff, 0xff, 0x7b,
    0xff, 0xff, 0x7b, 0xfb, 0xff, 0x7b, 0xf3, 0xff, 0x7b, 0xe8, 0x00, 0xfb,
    0xdf, 0xff, 0xfb, 0xbf, 0xff, 0xfb, 0xbf, 0xff, 0xfb, 0xdf, 0xff, 0xfb,
    0xe8, 0x00, 0x07, 0xf3, 0xff, 0xff, 0xfb, 0xff, 0xff, 0x7f, 0xff, 0xfe, 
  },
  /*    #f
   aaaaaaaaaabbbbbbbbbbcccccccccc
  --------++++++++--------++++++++
   ############################## 
  ################################
  ################################
  ##        ###      #####     ###
  ## ######### ###### ### ##### ##
  ## ######### ######### #########
  ## ######### ######### #########
  ## ########## ######## #########
  ##      ######    #### #########
  ## ############### ### #########
  ## ################ ## #########
  ## ################ ## #########
  ## ######### ###### ### ##### ##
  ##        ###      #####     ###
  ################################
   ############################## 
  */
  { 32, 16,
    0x7f, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x38, 0x1f, 0x07,
    0xdf, 0xf7, 0xee, 0xfb, 0xdf, 0xf7, 0xfd, 0xff, 0xdf, 0xf7, 0xfd, 0xff, 0xdf, 0xfb, 0xfd, 0xff,
    0xc0, 0xfc, 0x3d, 0xff, 0xdf, 0xff, 0xdd, 0xff, 0xdf, 0xff, 0xed, 0xff, 0xdf, 0xff, 0xed, 0xff,
    0xdf, 0xf7, 0xee, 0xfb, 0xc0, 0x38, 0x1f, 0x07, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xfe,
  },
  /*    #g
   aaaaaaaaaabbbbbbbbbbcccccccccc
  --------++++++++--------++++++++
   ############################## 
  ################################
  ################################
  ##         ####  #####      ####
  ###### ###### #### ### ##### ###
  ###### ###### #### ### ###### ##
  ###### ##### ###### ## ###### ##
  ###### ##### ###### ## ##### ###
  ###### ##### ###### ##      ####
  ###### #####        ## ##### ###
  ###### ##### ###### ## ###### ##
  ###### ##### ###### ## ###### ##
  ###### ##### ###### ## ##### ###
  ###### ##### ###### ##      ####
  ################################
   ############################## 
  */
  { 32, 16,
    0x7f, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x1e, 0x7c, 0x0f,
    0xfd, 0xfb, 0xdd, 0xf7, 0xfd, 0xfb, 0xdd, 0xfb, 0xfd, 0xf7, 0xed, 0xfb, 0xfd, 0xf7, 0xed, 0xf7,
    0xfd, 0xf7, 0xec, 0x0f, 0xfd, 0xf0, 0x0d, 0xf7, 0xfd, 0xf7, 0xed, 0xfb, 0xfd, 0xf7, 0xed, 0xfb,
    0xfd, 0xf7, 0xed, 0xf7, 0xfd, 0xf7, 0xec, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xfe, 
  },
  /*    #h
  --------++++++++
   ############## 
  ################
  ## ######### ###
  ### ####### ####
  #### ##### #####
  ##### ### ######
  ###### # #######
  ####### ########
  ####### ########
  ####### ########
  ####### ########
  ####### ########
  ####### ########
  ####### ########
  ################
   ############## 
  */
  { 16, 16,
    0x7f, 0xfe, 0xff, 0xff, 0xdf, 0xf7, 0xef, 0xef,
    0xf7, 0xdf, 0xfb, 0xbf, 0xfd, 0x7f, 0xfe, 0xff,
    0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
    0xfe, 0xff, 0xfe, 0xff, 0xff, 0xff, 0x7f, 0xfe,
  },
  /*    #i
  --------++++++++
   ############## 
  ################
  ## ######### ###
  ##  ######## ###
  ## # ####### ###
  ## ## ###### ###
  ## ### ##### ###
  ## #### #### ###
  ## ##### ### ###
  ## ###### ## ###
  ## ####### # ###
  ## ########  ###
  ## ######### ###
  ## ######### ###
  ################
   ############## 
  */
  { 16, 16,
    0x7f, 0xfe, 0xff, 0xff, 0xdf, 0xf7, 0xcf, 0xf7,
    0xd7, 0xf7, 0xdb, 0xf7, 0xdd, 0xf7, 0xde, 0xf7,
    0xdf, 0x77, 0xdf, 0xb7, 0xdf, 0xd7, 0xdf, 0xe7,
    0xdf, 0xf7, 0xdf, 0xf7, 0xff, 0xff, 0x7f, 0xfe, 
  },
};

#endif /* _SETTINGUIPAT_H_ */
