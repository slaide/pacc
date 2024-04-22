#include<tokenizer.h>

#include<util/util.h>

#include<stdlib.h>
#include<string.h>

bool Token_equalToken(Token* a,Token*b){
	if(a==b){
		return true;
	}
	if(a==nullptr || b==nullptr){
		return false;
	}

	if(a->len!=b->len)
		return false;

	return strncmp(a->p,b->p,a->len)==0;
}
bool Token_equalString(Token* a,char* b){
	int blen=strlen(b);
	if(a->len!=blen)
		return false;

	return strncmp(a->p,b,a->len)==0;
}

// map to keywords
void Token_map(Token*token){
	const char*KEYWORDS[]={
		KEYWORD_SWITCH,
		KEYWORD_CASE,
		KEYWORD_RETURN,
		KEYWORD_BREAK,
		KEYWORD_CONTINUE,
		KEYWORD_GOTO,
		KEYWORD_TYPEDEF,

		KEYWORD_STRUCT,
		KEYWORD_UNION,
		KEYWORD_ENUM,

        KEYWORD_INCLUDE,
		KEYWORD_DEFINE,
		KEYWORD_IFDEF,
		KEYWORD_IFNDEF,
		KEYWORD_UNDEF,
		KEYWORD_PRAGMA,

		KEYWORD_IF,
		KEYWORD_ELSE,
		KEYWORD_FOR,
		KEYWORD_WHILE,

		//KEYWORD_VOID,
		//KEYWORD_INT,
		//KEYWORD_FLOAT,
		//KEYWORD_DOUBLE,
		//KEYWORD_CHAR,

		//KEYWORD_LONG,
		//KEYWORD_UNSIGNED,

		KEYWORD_ASTERISK,
		KEYWORD_PLUS,
		KEYWORD_MINUS,
		KEYWORD_EQUAL,
		KEYWORD_QUESTIONMARK,
		KEYWORD_BANG,
		KEYWORD_HASH,
		KEYWORD_PARENS_OPEN,
		KEYWORD_PARENS_CLOSE,
		KEYWORD_SQUARE_BRACKETS_OPEN,
		KEYWORD_SQUARE_BRACKETS_CLOSE,
		KEYWORD_CURLY_BRACES_OPEN,
		KEYWORD_CURLY_BRACES_CLOSE,
		KEYWORD_COLON,
		KEYWORD_SEMICOLON,
		KEYWORD_COMMA,
		KEYWORD_AMPERSAND,
	};

	static const int NUM_KEYWORDS=sizeof(KEYWORDS)/sizeof(KEYWORDS[0]);

	for(int i=0;i<NUM_KEYWORDS;i++){
		if(strlen(KEYWORDS[i])==token->len){
			if(strncmp(token->p,KEYWORDS[i],token->len)==0){
				token->p=KEYWORDS[i];
				token->tag=TOKEN_TAG_KEYWORD;
				return;
			}
		}
	}
}

bool char_is_token(char c){
	static const char*CHAR_TOKENS="()[]{},;.:-+*~#'\"\\/!?%&=<>|";
	for(int i=0;i<strlen(CHAR_TOKENS);i++){
		if(CHAR_TOKENS[i]==c){
			return true;
		}
	}
	return false;
}


int Tokenizer_init(Tokenizer tokenizer[static 1],File file[static 1]){
	*tokenizer=(Tokenizer){
		.token_src=file->filepath,
		.num_tokens=0,
		.tokens=nullptr
	};

	int line=0;
	int col=0;

	char*p=file->contents;
	char*const end=file->contents+file->contents_len;

	// parse token one at a time
	while(1){
		Token token={
			.filename=nullptr,
			.line=line+1,
			.col=col,
			.p=p,
			.tag=TOKEN_TAG_UNDEFINED,
		};

		// parse characters
		while(p<end){
			switch((int)*p){
				// end of file
				case 0:
				case EOF:
					if(token.p==p){
						goto tokenizer_init_ret;
					}
					goto token_end;
					break;
				// new line
				case '\n':
					if(token.p==p){
						line++;
						col=0;
						p++;
						token.line=line+1;
						token.col=col;
						token.p=p;
						continue;
					}
					goto token_end;
					break;
				// breaking whitespace
				case '\r':
				case '\t':
				case ' ':
					if(token.p==p){
						col++;
						p++;
						token.col=col;
						token.p=p;
						continue;
					}
					goto token_end;
					break;
				// any other character
				default:
					// special characters, like semicolon, paranthesis etc.
					if(char_is_token(*p)){
						// if this is the only char in the current token, it IS the current token
						// -> advance p past it for next iteration, and finish token
						if(token.p==p){
							col++;
							p++;
						}
						// if this is not the only char in the current token, do not advance p past it
						// but still finish the current token
						goto token_end;
					}
					goto token_continue;
			}

			token_end:
				token.len=p-token.p;
				break;

			token_continue:
				p++;
				col++;
				continue;
		}

		// check for compound tokens

		// 1) string literals
		if(token.len==1 && token.p[0]=='"'){
			token.tag=TOKEN_TAG_LITERAL_STRING;

			// include all characters until next quotation mark
			// note escaped quotation mark though (which is part of the string, does not terminate it)
			while(p<end){
				if(*p=='"'){
					if(*(p-1)!='\\'){
						break;
					}
				}
				p++;
			}
			// advance past trailing "
			p++;

			token.len=p-token.p;
		}

		// 2) character literal
		if(token.len==1 && token.p[0]=='\''){
			token.tag=TOKEN_TAG_LITERAL_CHAR;

			// check escaped character
			if(*p=='\\'){
				p++;
			}

			// skip actual character literal
			p++;

			// end character literal with trailing single quotation mark
			if(*p!='\''){
				// TODO error
			}
			p++;

			token.len=p-token.p;
		}

		// 3) comment compound tokens
		if(token.len==1 && tokenizer->num_tokens>0 && tokenizer->tokens[tokenizer->num_tokens-1].len==1){
			Token*last_token=&tokenizer->tokens[tokenizer->num_tokens-1];
			if(last_token->p[0]=='/'){
				if(token.p[0]=='/'){
					// begin comment -> parse until end of line
					last_token->tag=TOKEN_TAG_COMMENT;

					while(*p!='\n' && p<end)
						p++;

					last_token->len=p-last_token->p;

					continue;
				}
				if(token.p[0]=='*'){
					// begin multiline comment -> parse until */
					last_token->tag=TOKEN_TAG_COMMENT;

					while(p<end){
						p++;
						if(*p=='*' && *(p+1)=='/'){
							p+=2;
							break;
						}
					}

					last_token->len=p-last_token->p;

					continue;
				}
			}
		}

		// 4) multi-character tokens
		if(token.len==1 && tokenizer->num_tokens>0 && tokenizer->tokens[tokenizer->num_tokens-1].len==1){
			Token*last_token=&tokenizer->tokens[tokenizer->num_tokens-1];
			if(
				// left shift
				(last_token->p[0]=='<' && token.p[0]=='<')
				||
				// right shift
				(last_token->p[0]=='>' && token.p[0]=='>')
				||
				// equality
				(last_token->p[0]=='=' && token.p[0]=='=')
				||
				// inequality
				(last_token->p[0]=='!' && token.p[0]=='=')
				||
				// less than or equal
				(last_token->p[0]=='<' && token.p[0]=='=')
				||
				// greater than or equal
				(last_token->p[0]=='>' && token.p[0]=='=')
				||
				// logical and
				(last_token->p[0]=='&' && token.p[0]=='&')
				||
				// logical or
				(last_token->p[0]=='|' && token.p[0]=='|')
				||
				// increment
				(last_token->p[0]=='+' && token.p[0]=='+')
				||
				// decrement
				(last_token->p[0]=='-' && token.p[0]=='-')
				||
				// arrow
				(last_token->p[0]=='-' && token.p[0]=='>')
			){
				last_token->len=2;
				last_token->tag=TOKEN_TAG_KEYWORD;

				continue;
			}
		}

		// check for number literals
		while(token.len>0){
			Token*numericToken=&token;

			// check for leading sign
			int hasSign=false;
			if(numericToken->p[0]=='-' || numericToken->p[0]=='+'){
				hasSign=true;
				/*if(numericToken->len==1)
					break;*/
			}

			int offset=hasSign;

			// check for prefix (0x, 0b, 0o)
			int hasPrefix=false;
			if(numericToken->p[offset]==0){
				if(numericToken->p[offset+1]=='x' || numericToken->p[offset+1]=='X'){
					hasPrefix=true;
				}
				if(numericToken->p[offset+1]=='b' || numericToken->p[offset+1]=='B'){
					hasPrefix=true;
				}
				if(numericToken->p[offset+1]=='o' || numericToken->p[offset+1]=='O'){
					hasPrefix=true;
				}
			}
			offset+=hasPrefix;

			// check for leading digit[s]
			int hasLeadingDigits=false;
			while(numericToken->p[offset]>='0' && numericToken->p[offset]<='9'){
				hasLeadingDigits=true;
				offset++;
			}

			// check for decimal point (which may be the leading character, omitting any leading digit, e.g. ".5")
			int hasDecimalPoint=false;
			if(numericToken->p[offset]=='.'){
				hasDecimalPoint=true;
				if(numericToken->len==offset+1)
					break;
			}
			offset+=hasDecimalPoint;

			// check for trailing digits (can only be present if there is a decimal point)
			int hasTrailingDigits=false;
			if(hasDecimalPoint){
				// check for trailing digits
				while(numericToken->p[offset]>='0' && numericToken->p[offset]<='9'){
					hasTrailingDigits=true;
					offset++;
				}
			}

			// check for exponent
			int hasExponent=false;
			if((numericToken->p[offset]=='e' || numericToken->p[offset]=='E')){
				hasExponent=true;
				offset++;
				if(offset==numericToken->len)
					break;
			}

			// check for exponent sign
			int hasExponentSign=false;
			if(hasExponent && (numericToken->p[offset]=='-' || numericToken->p[offset]=='+')){
				hasExponentSign=true;
				offset++;
				if(offset==numericToken->len)
					break;
			}

			// check for exponent digits (exponent must have at least one digit, and be base10 integer)
			int hasExponentDigits=false;
			while(numericToken->p[offset]>='0' && numericToken->p[offset]<='9'){
				hasExponentDigits=true;
				offset++;
			}

			// check if all characters in token have been processed (if not, this is not a valid number)
			if(offset<numericToken->len)
				break;

			if(hasExponent && !hasExponentDigits){
				fatal("invalid number literal");
			}

			// adjust token based on symbols in its vicinity
			numericToken->len=offset;
			p+=offset-1;

			// check for flags to determine number type
			if(hasDecimalPoint || hasExponent){
				numericToken->tag=TOKEN_TAG_LITERAL_FLOAT;
			}else{
				numericToken->tag=TOKEN_TAG_LITERAL_INTEGER;
			}

			break;
		}

        // parse preprocessor include directive argument which functions as string (i.e. do not strip whitespace)
        if(token.len==1 && token.p[0]=='<' && tokenizer->num_tokens>=2){
            if(
                // if two preceding tokens are # and include
                // and the hash is either in the first line, or the start of the line
                (
                    tokenizer->num_tokens==2
                    ||
                    (
                        tokenizer->tokens[tokenizer->num_tokens-3].line
                        <
                        tokenizer->tokens[tokenizer->num_tokens-2].line
                    )
                )
                &&
                Token_equalString(&tokenizer->tokens[tokenizer->num_tokens-2],KEYWORD_HASH)
                &&
                Token_equalString(&tokenizer->tokens[tokenizer->num_tokens-1],KEYWORD_INCLUDE)
            ){
                token.tag=TOKEN_TAG_PREP_INCLUDE_ARGUMENT;

                while(*p!='>' && p<end){
                    p++;
                }
				if(p<end)
                	p++;

                token.len=p-token.p;
            }
        }

		// if token is empty, break (e.g. if EOF reached)
        if(token.len==0){
            break;
        }

		tokenizer->num_tokens++;
		tokenizer->tokens=realloc(tokenizer->tokens,(tokenizer->num_tokens)*sizeof(Token));

		tokenizer->tokens[tokenizer->num_tokens-1]=token;
	}

	tokenizer_init_ret:
		// go through tokens and make sure none are undefined
		for(int i=0;i<tokenizer->num_tokens;i++){

			Token_map(&tokenizer->tokens[i]);

			if(tokenizer->tokens[i].tag!=TOKEN_TAG_UNDEFINED){
				continue;
			}

			// if token is undefined, check if is a symbol
			if(
				(tokenizer->tokens[i].p[0]>='a' && tokenizer->tokens[i].p[0]<='z')
				||
				(tokenizer->tokens[i].p[0]>='A' && tokenizer->tokens[i].p[0]<='Z')
				||
				(tokenizer->tokens[i].p[0]=='_')
			){
				tokenizer->tokens[i].tag=TOKEN_TAG_SYMBOL;
				continue;
			}

			if(char_is_token(tokenizer->tokens[i].p[0])){
				tokenizer->tokens[i].tag=TOKEN_TAG_KEYWORD;
				continue;
			}
			
			fatal("undefined token %.*s in line %d col %d",tokenizer->tokens[i].len,tokenizer->tokens[i].p,tokenizer->tokens[i].line,tokenizer->tokens[i].col);
		}
		return tokenizer->num_tokens;
}

void TokenIter_init(
    struct TokenIter*token_iter,
    Tokenizer*tokenizer,
    struct TokenIterConfig config
){
    token_iter->config=config;
    token_iter->tokenizer=tokenizer;

    token_iter->next_token_index=0;
}
int TokenIter_nextToken(struct TokenIter*iter,Token*out){
    if(iter->next_token_index>=iter->tokenizer->num_tokens){
        return false;
    }

    *out=iter->tokenizer->tokens[iter->next_token_index++];

    if(iter->config.skip_comments && out->tag==TOKEN_TAG_COMMENT)
        return TokenIter_nextToken(iter,out);

    return true;
}
int TokenIter_lastToken(struct TokenIter*iter,Token*out){
	if(iter->next_token_index<=0){
		return false;
	}
	
	*out=iter->tokenizer->tokens[iter->next_token_index-1];

	return true;
}
bool TokenIter_isEmpty(struct TokenIter*iter){
	return iter->next_token_index>=iter->tokenizer->num_tokens;
}
