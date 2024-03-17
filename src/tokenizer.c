#include<tokenizer.h>

#include<util.h>

#include<stdlib.h>
#include<string.h>

bool Token_symbolEqual(Token* a,Token*b){
	if(a->len!=b->len)
		return false;

	return strncmp(a->p,b->p,a->len)==0;
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

		KEYWORD_VOID,
		KEYWORD_INT,
		KEYWORD_FLOAT,
		KEYWORD_DOUBLE,
		KEYWORD_CHAR,

		KEYWORD_LONG,
		KEYWORD_UNSIGNED,

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
	};

	static const int NUM_KEYWORDS=sizeof(KEYWORDS)/sizeof(KEYWORDS[0]);

	for(int i=0;i<NUM_KEYWORDS;i++){
		if(strlen(KEYWORDS[i])==token->len){
			if(strncmp(token->p,KEYWORDS[i],token->len)==0){
				token->p=KEYWORDS[i];
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


int Tokenizer_init(Tokenizer*tokenizer,File*file){
	int line=0;
	int col=0;

	char*p=file->contents;

	// parse token one at a time
	while(1){
		Token token={
			.filename=nullptr,
			.line=line,
			.col=col,
			.p=p,
		};

		// parse characters
		while(1){
			switch((int)*p){
				// end of file
				case 0:
				case EOF:
					if(token.p==p){
						return tokenizer->num_tokens;
					}
					goto token_end;
					break;
				// new line
				case '\n':
					if(token.p==p){
						line++;
						col=0;
						p++;
						token.line=line;
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
						token.tag=TOKEN_TAG_KEYWORD;
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

		// check for string literals
		if(token.len==1 && token.p[0]=='"'){
			token.tag=TOKEN_TAG_LITERAL_STRING;

			// include all characters until next quotation mark
			// note escaped quotation mark though (which is part of the string, does not terminate it)
			while(1){
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

		// check for character literal
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

		// check for compound tokens
		if(token.len==1 && tokenizer->num_tokens>0 && tokenizer->tokens[tokenizer->num_tokens-1].len==1){
			Token*last_token=&tokenizer->tokens[tokenizer->num_tokens-1];
			if(last_token->p[0]=='/'){
				if(token.p[0]=='/'){
					// begin comment -> parse until end of line
					last_token->tag=TOKEN_TAG_COMMENT;

					while(*p!='\n')
						p++;

					last_token->len=p-last_token->p;

					continue;
				}
				if(token.p[0]=='*'){
					// begin multiline comment -> parse until */
					last_token->tag=TOKEN_TAG_COMMENT;

					while(1){
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

        // parse preprocessor include directive argument which functions as string (i.e. do not strip whtiespace)
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
                tokenizer->tokens[tokenizer->num_tokens-2].p==KEYWORD_HASH
                &&
                tokenizer->tokens[tokenizer->num_tokens-1].p==KEYWORD_INCLUDE
            ){
                token.tag=TOKEN_TAG_PREP_INCLUDE_ARGUMENT;

                while(*p!='>'){
                    p++;
                }
                p++;

                token.len=p-token.p;
            }
        }

        if(token.len==0){
            fatal("how did this happen!? line %d col %d \n",line,col);
        }

		Token_map(&token);

		tokenizer->num_tokens++;
		tokenizer->tokens=realloc(tokenizer->tokens,(tokenizer->num_tokens)*sizeof(Token));

		tokenizer->tokens[tokenizer->num_tokens-1]=token;
	}
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
