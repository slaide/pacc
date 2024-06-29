#define F_WITH_2ARGS(A,B) A,B
#define F(T,B,...) T (F_WITH_2ARGS(__VA_ARGS__)) B
F( int main , {return c>=1;}, int c, char**v )
