/*
   base64.cpp and base64.h

   Copyright (C) 2004-2008 René Nyffenegger

   This source code is provided 'as-is', without any express or implied
   warranty. In no event will the author be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this source code must not be misrepresented; you must not
	  claim that you wrote the original source code. If you use this source code
	  in a product, an acknowledgment in the product documentation would be
	  appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
	  misrepresented as being the original source code.

   3. This notice may not be removed or altered from any source distribution.

   René Nyffenegger rene.nyffenegger@adp-gmbh.ch

*/
#include "common.h"
#include <iostream>
#include <string>
#include <vector>

static const std::string base64_chars =
			 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			 "abcdefghijklmnopqrstuvwxyz"
			 "0123456789+/";


static inline bool is_base64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/'));
}



/* Define these if they aren't already in your environment
 * #define TEXT(x) Lx    //Unicode
 * #define TCHAR wchar_t //Unicode
 * #define TCHAR char    //Not unicode
 * #define TEXT(x) x     //Not unicode
 * #define DWORD long
 * #define BYTE unsigned char
 * They are defined by default in Windows.h
 */

//Lookup table for encoding
//If you want to use an alternate alphabet, change the characters here
const static unsigned char encodeLookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const static unsigned char padCharacter = '=';
std::string base64Encode(const unsigned char *inputBuffer, size_t size)
{
		std::basic_string<char> encodedString;
		encodedString.reserve(((size/3) + (size % 3 > 0)) * 4);
		uint32_t temp;

		const unsigned char *cursor = inputBuffer;
		for(size_t idx = 0; idx < size/3; idx++)
		{
				temp  = (*cursor++) << 16; //Convert to big endian
				temp += (*cursor++) << 8;
				temp += (*cursor++);
				encodedString.append(1,encodeLookup[(temp & 0x00FC0000) >> 18]);
				encodedString.append(1,encodeLookup[(temp & 0x0003F000) >> 12]);
				encodedString.append(1,encodeLookup[(temp & 0x00000FC0) >> 6 ]);
				encodedString.append(1,encodeLookup[(temp & 0x0000003F)      ]);
		}
		switch(size % 3)
		{
		case 1:
				temp  = (*cursor++) << 16; //Convert to big endian
				encodedString.append(1,encodeLookup[(temp & 0x00FC0000) >> 18]);
				encodedString.append(1,encodeLookup[(temp & 0x0003F000) >> 12]);
				encodedString.append(2,padCharacter);
				break;
		case 2:
				temp  = (*cursor++) << 16; //Convert to big endian
				temp += (*cursor++) << 8;
				encodedString.append(1,encodeLookup[(temp & 0x00FC0000) >> 18]);
				encodedString.append(1,encodeLookup[(temp & 0x0003F000) >> 12]);
				encodedString.append(1,encodeLookup[(temp & 0x00000FC0) >> 6 ]);
				encodedString.append(1,padCharacter);
				break;
		}
		return encodedString;
}

std::string es3::base64_encode(const char *str, size_t in_len)
{
	return base64Encode((const unsigned char*)str, in_len);
}

std::string es3::base64_decode(const std::string &encoded_string) {
  size_t in_len = encoded_string.size();
  size_t i = 0;
  size_t j = 0;
  size_t in_ = 0;
  unsigned char char_array_4[4], char_array_3[3];
  std::string ret;

  while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
	char_array_4[i++] = encoded_string[in_]; in_++;
	if (i ==4) {
	  for (i = 0; i <4; i++)
		char_array_4[i] = base64_chars.find(char_array_4[i]);

	  char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
	  char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
	  char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

	  for (i = 0; (i < 3); i++)
		ret += char_array_3[i];
	  i = 0;
	}
  }

  if (i) {
	for (j = i; j <4; j++)
	  char_array_4[j] = 0;

	for (j = 0; j <4; j++)
	  char_array_4[j] = base64_chars.find(char_array_4[j]);

	char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
	char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
	char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

	for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
  }

  return ret;
}
