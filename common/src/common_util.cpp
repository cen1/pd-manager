/*
Copyright [2025] [cen1]

  Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "common_util.h"

vector<string> UTIL_Tokenize(string s, char delim)
{
	vector<string> Tokens;
	string Token;

	for (string::iterator i = s.begin(); i != s.end(); ++i) {
		if (*i == delim) {
			if (Token.empty())
				continue;

			Tokens.push_back(Token);
			Token.clear();
		}
		else
			Token += *i;
	}

	if (!Token.empty())
		Tokens.push_back(Token);

	return Tokens;
}
