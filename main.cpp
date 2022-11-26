#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace std;

/*
{ Sample program
  in TINY language
  compute factorial
}

read x; {input an integer}
if 0<x then {compute only if x>=1}
  fact:=1;
  repeat
    fact := fact * x;
    x:=x-1
  until x=0;
  write fact {output factorial}
end
*/

// sequence of statements separated by ;
// no procedures - no declarations
// all variables are integers
// variables are declared simply by assigning values to them :=
// if-statement: if (boolean) then [else] end
// repeat-statement: repeat until (boolean)
// boolean only in if and repeat conditions < = and two mathematical expressions
// math expressions integers only, + - * / ^
// I/O read write
// Comments {}

////////////////////////////////////////////////////////////////////////////////////
// Strings /////////////////////////////////////////////////////////////////////////

bool Equals(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

bool StartsWith(const char *a, const char *b) {
    int nb = strlen(b);
    return strncmp(a, b, nb) == 0;
}

void Copy(char *a, const char *b, int n = 0) {
    if (n > 0) {
        strncpy(a, b, n);
        a[n] = 0;
    }
    else strcpy(a, b);
}

void AllocateAndCopy(char **a, const char *b) {
    if (b == 0) {
        *a = 0;
        return;
    }
    int n = strlen(b);
    *a = new char[n + 1];
    strcpy(*a, b);
}

////////////////////////////////////////////////////////////////////////////////////
// Input and Output ////////////////////////////////////////////////////////////////

#define MAX_LINE_LENGTH 10000

struct InFile {
    FILE *file;
    int cur_line_num;

    char line_buf[MAX_LINE_LENGTH];
    int cur_ind, cur_line_size;

    InFile(const char *str) {
        file = 0;
        if (str) file = fopen(str, "r");
        cur_line_size = 0;
        cur_ind = 0;
        cur_line_num = 0;
    }

    ~InFile() { if (file) fclose(file); }

    void SkipSpaces() {
        while (cur_ind < cur_line_size) {
            char ch = line_buf[cur_ind];
            if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') break;
            cur_ind++;
        }
    }

    bool SkipUpto(const char *str) {
        while (true) {
            SkipSpaces();
            while (cur_ind >= cur_line_size) {
                if (!GetNewLine()) return false;
                SkipSpaces();
            }

            if (StartsWith(&line_buf[cur_ind], str)) {
                cur_ind += strlen(str);
                return true;
            }
            cur_ind++;
        }
        return false;
    }

    bool GetNewLine() {
        cur_ind = 0;
        line_buf[0] = 0;
        if (!fgets(line_buf, MAX_LINE_LENGTH, file)) return false;
        cur_line_size = strlen(line_buf);
        if (cur_line_size == 0) return false; // End of file
        cur_line_num++;
        return true;
    }

    char *GetNextTokenStr() {
        SkipSpaces();
        while (cur_ind >= cur_line_size) {
            if (!GetNewLine()) return 0;
            SkipSpaces();
        }
        return &line_buf[cur_ind];
    }

    void Advance(int num) {
        cur_ind += num;
    }
};

struct OutFile {
    FILE *file;

    OutFile(const char *str) {
        file = 0;
        if (str) file = fopen(str, "w");
    }

    ~OutFile() { if (file) fclose(file); }

    void Out(const char *s) {
        fprintf(file, "%s\n", s);
        fflush(file);
    }
};

////////////////////////////////////////////////////////////////////////////////////
// Compiler Parameters /////////////////////////////////////////////////////////////

struct CompilerInfo {
    InFile in_file;
    OutFile out_file;
    OutFile debug_file;

    CompilerInfo(const char *in_str, const char *out_str, const char *debug_str)
            : in_file(in_str), out_file(out_str), debug_file(debug_str) {
    }
};

////////////////////////////////////////////////////////////////////////////////////
// Scanner /////////////////////////////////////////////////////////////////////////

#define MAX_TOKEN_LEN 40

enum TokenType {
    IF, THEN, ELSE, END, REPEAT, UNTIL, READ, WRITE,
    ASSIGN, EQUAL, LESS_THAN,
    PLUS, MINUS, TIMES, DIVIDE, POWER,
    SEMI_COLON,
    LEFT_PAREN, RIGHT_PAREN,
    LEFT_BRACE, RIGHT_BRACE,
    ID, NUM,
    ENDFILE, ERROR
};

// Used for debugging only /////////////////////////////////////////////////////////
const char *TokenTypeStr[] =
        {
                "If", "Then", "Else", "End", "Repeat", "Until", "Read", "Write",
                "Assign", "Equal", "LessThan",
                "Plus", "Minus", "Times", "Divide", "Power",
                "SemiColon",
                "LeftParen", "RightParen",
                "LeftBrace", "RightBrace",
                "ID", "Num",
                "EndFile", "Error"
        };

struct Token {
    TokenType type;
    char str[MAX_TOKEN_LEN + 1];

    Token() {
        str[0] = 0;
        type = ERROR;
    }

    Token(TokenType _type, const char *_str) {
        type = _type;
        Copy(str, _str);
    }
};

const Token reserved_words[] =
        {
                Token(IF, "if"),
                Token(THEN, "then"),
                Token(ELSE, "else"),
                Token(END, "end"),
                Token(REPEAT, "repeat"),
                Token(UNTIL, "until"),
                Token(READ, "read"),
                Token(WRITE, "write")
        };
const int num_reserved_words = sizeof(reserved_words) / sizeof(reserved_words[0]);

// if there is tokens like < <=, sort them such that sub-tokens come last: <= <
// the closing comment should come immediately after opening comment
const Token symbolic_tokens[] =
        {
                Token(ASSIGN, ":="),
                Token(EQUAL, "="),
                Token(LESS_THAN, "<"),
                Token(PLUS, "+"),
                Token(MINUS, "-"),
                Token(TIMES, "*"),
                Token(DIVIDE, "/"),
                Token(POWER, "^"),
                Token(SEMI_COLON, ";"),
                Token(LEFT_PAREN, "("),
                Token(RIGHT_PAREN, ")"),
                Token(LEFT_BRACE, "{"),
                Token(RIGHT_BRACE, "}")
        };
const int num_symbolic_tokens = sizeof(symbolic_tokens) / sizeof(symbolic_tokens[0]);

inline bool IsDigit(char ch) { return (ch >= '0' && ch <= '9'); }

inline bool IsLetter(char ch) { return ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')); }

inline bool IsLetterOrUnderscore(char ch) { return (IsLetter(ch) || ch == '_'); }

void GetNextToken(CompilerInfo *pci, Token *ptoken) {
    ptoken->type = ERROR;
    ptoken->str[0] = 0;

    int i;
    char *s = pci->in_file.GetNextTokenStr();
    if (!s) {
        ptoken->type = ENDFILE;
        ptoken->str[0] = 0;
        return;
    }

    for (i = 0; i < num_symbolic_tokens; i++) {
        if (StartsWith(s, symbolic_tokens[i].str))
            break;
    }

    if (i < num_symbolic_tokens) {
        if (symbolic_tokens[i].type == LEFT_BRACE) {
            pci->in_file.Advance(strlen(symbolic_tokens[i].str));
            if (!pci->in_file.SkipUpto(symbolic_tokens[i + 1].str)) return;
            return GetNextToken(pci, ptoken);
        }
        ptoken->type = symbolic_tokens[i].type;
        Copy(ptoken->str, symbolic_tokens[i].str);
    } else if (IsDigit(s[0])) {
        int j = 1;
        while (IsDigit(s[j])) j++;

        ptoken->type = NUM;
        Copy(ptoken->str, s, j);
    } else if (IsLetterOrUnderscore(s[0])) {
        int j = 1;
        while (IsLetterOrUnderscore(s[j])) j++;

        ptoken->type = ID;
        Copy(ptoken->str, s, j);

        for (i = 0; i < num_reserved_words; i++) {
            if (Equals(ptoken->str, reserved_words[i].str)) {
                ptoken->type = reserved_words[i].type;
                break;
            }
        }
    }

    int len = strlen(ptoken->str);
    if (len > 0) pci->in_file.Advance(len);
}

////////////////////////////////////////////////////////////////////////////////////
// Parser //////////////////////////////////////////////////////////////////////////

// program -> stmtseq
// stmtseq -> stmt { ; stmt }
// stmt -> ifstmt | repeatstmt | assignstmt | readstmt | writestmt
// ifstmt -> if exp then stmtseq [ else stmtseq ] end
// repeatstmt -> repeat stmtseq until expr
// assignstmt -> identifier := expr
// readstmt -> read identifier
// writestmt -> write expr
// expr -> mathexpr [ (< | =) mathexpr ]
// mathexpr -> term { (+|-) term }    left associative
// term -> factor { (*|/) factor }    left associative
// factor -> newexpr { ^ newexpr }    right associative
// newexpr -> ( mathexpr ) | number | identifier

enum NodeKind {
    IF_NODE, REPEAT_NODE, ASSIGN_NODE, READ_NODE, WRITE_NODE,
    OPER_NODE, NUM_NODE, ID_NODE
};

// Used for debugging only /////////////////////////////////////////////////////////
const char *NodeKindStr[] =
        {
                "If", "Repeat", "Assign", "Read", "Write",
                "Oper", "Num", "ID"
        };

enum ExprDataType {
    VOID, INTEGER, BOOLEAN
};

// Used for debugging only /////////////////////////////////////////////////////////
const char *ExprDataTypeStr[] =
        {
                "Void", "Integer", "Boolean"
        };

#define MAX_CHILDREN 3

struct TreeNode {
    TreeNode *child[MAX_CHILDREN];
    TreeNode *sibling; // used for sibling statements only

    NodeKind node_kind;

    union {
        TokenType oper;
        int num;
        char *id;
    }; // defined for expression/int/identifier only
    ExprDataType expr_data_type; // defined for expression/int/identifier only

    int line_num;

    TreeNode() {
        int i;
        for (i = 0; i < MAX_CHILDREN; i++) child[i] = 0;
        sibling = 0;
        expr_data_type = VOID;
    }
};

struct ParseInfo {
    Token next_token;
};

void PrintTree(TreeNode *node, int sh = 0) {
    int i, NSH = 3;
    for (i = 0; i < sh; i++) printf(" ");

    printf("[%s]", NodeKindStr[node->node_kind]);

    if (node->node_kind == OPER_NODE) printf("[%s]", TokenTypeStr[node->oper]);
    else if (node->node_kind == NUM_NODE) printf("[%d]", node->num);
    else if (node->node_kind == ID_NODE || node->node_kind == READ_NODE || node->node_kind == ASSIGN_NODE)
        printf("[%s]", node->id);

    if (node->expr_data_type != VOID) printf("[%s]", ExprDataTypeStr[node->expr_data_type]);

    printf("\n");

    for (i = 0; i < MAX_CHILDREN; i++) if (node->child[i]) PrintTree(node->child[i], sh + NSH);
    if (node->sibling) PrintTree(node->sibling, sh);
}

struct ParseTree{
    CompilerInfo *compilerInfo;
    ParseInfo *parseInfo;
    TreeNode *root;

    ParseTree(CompilerInfo *ci){
        compilerInfo = ci;
        parseInfo = new ParseInfo;
        GetNextToken(compilerInfo, &parseInfo->next_token);
        root = RDParseProgram();
    }

    //Build the parse tree recursively
    TreeNode *RDParseProgram();
    TreeNode *RDParseStmtSeq();
    TreeNode *RDParseStmt();
    TreeNode *RDParseIfStmt();
    TreeNode *RDParseRepeatStmt();
    TreeNode *RDParseAssignmentStmt();
    TreeNode *RDParseReadStmt();
    TreeNode *RDParseWriteStmt();
    TreeNode *RDParseExpr();
    TreeNode *RDParseMathExpr();
    TreeNode *RDParseTerm();
    TreeNode *RDParseFactor();
    TreeNode *RDParseNewExp();

    //Match token type with current token, if worked properly nothing happens
    //In case of error an error statement is printed to the terminal
    void Match(const TokenType&);
};

void PrintError(const char* str){
    printf("%s\n", str);
}

TreeNode *ParseTree::RDParseProgram() {
    TreeNode *currNode = RDParseStmtSeq();
    return currNode;
}

TreeNode *ParseTree::RDParseStmtSeq() {
    TreeNode *currNode = RDParseStmt();
    TreeNode *currSibling = currNode;
    while (parseInfo->next_token.type == SEMI_COLON){
        Match(SEMI_COLON);
        TreeNode *nextSibling = RDParseStmt();
        if (nextSibling){
            while (currSibling->sibling) currSibling = currSibling->sibling;
            currSibling->sibling = nextSibling;
        }
    }
    return currNode;
}

TreeNode *ParseTree::RDParseStmt() {
    TreeNode *currNode = nullptr;
    switch (parseInfo->next_token.type) {
        case IF:
            currNode = RDParseIfStmt();
            break;
        case REPEAT:
            currNode = RDParseRepeatStmt();
            break;
        case ID:
            currNode = RDParseAssignmentStmt();
            break;
        case READ:
            currNode = RDParseReadStmt();
            break;
        case WRITE:
            currNode = RDParseWriteStmt();
            break;
        default:
            char errorMessage[100];
            sprintf(errorMessage, "Unexpected Statement at line %d", compilerInfo->in_file.cur_line_num);
            PrintError(errorMessage);
            break;
    }
    return currNode;
}

TreeNode *ParseTree::RDParseIfStmt() {
    Match(IF);
    TreeNode *currNode = new TreeNode;
    currNode->node_kind = IF_NODE;
    currNode->line_num = compilerInfo->in_file.cur_line_num;
    currNode->child[0] = RDParseExpr();
    Match(THEN);
    currNode->child[1] = RDParseStmtSeq();
    if (parseInfo->next_token.type == ELSE){
        Match(ELSE);
        currNode->child[2] = RDParseStmtSeq();
    }
    Match(END);
    return currNode;
}

TreeNode *ParseTree::RDParseRepeatStmt() {
    Match(REPEAT);
    TreeNode *currNode = new TreeNode;
    currNode->node_kind = REPEAT_NODE;
    currNode->line_num = compilerInfo->in_file.cur_line_num;
    currNode->child[0] = RDParseStmtSeq();
    Match(UNTIL);
    currNode->child[1] = RDParseExpr();
    return currNode;
}

TreeNode *ParseTree::RDParseAssignmentStmt() {
    TreeNode *currNode = new TreeNode;
    currNode->node_kind = ASSIGN_NODE;
    currNode->line_num = compilerInfo->in_file.cur_line_num;
    currNode->expr_data_type = VOID;
    currNode->id = new char[MAX_TOKEN_LEN];
    strcpy(currNode->id, parseInfo->next_token.str);
    Match(ID);
    Match(ASSIGN);
    currNode->child[0] = RDParseExpr();
    return currNode;
}

TreeNode *ParseTree::RDParseReadStmt() {
    Match(READ);
    TreeNode *currNode = new TreeNode;
    currNode->line_num = compilerInfo->in_file.cur_line_num;
    currNode->node_kind = READ_NODE;
    currNode->id = new char[MAX_TOKEN_LEN];
    strcpy(currNode->id, parseInfo->next_token.str);
    Match(ID);
    return currNode;
}

TreeNode *ParseTree::RDParseWriteStmt() {
    Match(WRITE);
    TreeNode *currNode = new TreeNode;
    currNode->line_num = compilerInfo->in_file.cur_line_num;
    currNode->node_kind = WRITE_NODE;
    currNode->id = new char[MAX_TOKEN_LEN];
    strcpy(currNode->id, parseInfo->next_token.str);
    Match(ID);
    return currNode;
}

TreeNode *ParseTree::RDParseExpr() {
    TreeNode *currNode = RDParseMathExpr();
    if (parseInfo->next_token.type == LESS_THAN || parseInfo->next_token.type == EQUAL){
        TreeNode *newNode = new TreeNode;
        newNode->oper = parseInfo->next_token.type;
        Match(newNode->oper);
        newNode->node_kind = OPER_NODE;
        newNode->line_num = compilerInfo->in_file.cur_line_num;
        newNode->expr_data_type = BOOLEAN;
        newNode->child[0] = currNode;
        newNode->child[1] = RDParseMathExpr();
        currNode = newNode;
    }
    return currNode;
}

TreeNode *ParseTree::RDParseMathExpr() {
    TreeNode *currNode = RDParseTerm();
    while (parseInfo->next_token.type == PLUS || parseInfo->next_token.type == MINUS){
        TreeNode *newNode = new TreeNode;
        newNode->oper = parseInfo->next_token.type;
        Match(newNode->oper);
        newNode->node_kind = OPER_NODE;
        newNode->line_num = compilerInfo->in_file.cur_line_num;
        newNode->expr_data_type = INTEGER;
        newNode->child[0] = currNode;
        newNode->child[1] = RDParseTerm();
        currNode = newNode;
    }
    return currNode;
}

TreeNode *ParseTree::RDParseTerm() {
    TreeNode *currNode = RDParseFactor();
    while (parseInfo->next_token.type == TIMES || parseInfo->next_token.type == DIVIDE){
        TreeNode *newNode = new TreeNode;
        newNode->oper = parseInfo->next_token.type;
        Match(newNode->oper);
        newNode->node_kind = OPER_NODE;
        newNode->line_num = compilerInfo->in_file.cur_line_num;
        newNode->expr_data_type = INTEGER;
        newNode->child[0] = currNode;
        newNode->child[1] = RDParseFactor();
        currNode = newNode;
    }
    return currNode;
}

TreeNode *ParseTree::RDParseFactor() {
    TreeNode *currNode = RDParseNewExp();
    while (parseInfo->next_token.type == POWER){
        TreeNode *newNode = new TreeNode;
        newNode->oper = parseInfo->next_token.type;
        Match(newNode->oper);
        newNode->node_kind = OPER_NODE;
        newNode->line_num = compilerInfo->in_file.cur_line_num;
        newNode->expr_data_type = INTEGER;
        newNode->child[1] = currNode;
        currNode = newNode;
        currNode->child[0] = RDParseNewExp();
    }
    return currNode;
}

TreeNode *ParseTree::RDParseNewExp() {
    TreeNode *currNode = nullptr;
    switch (parseInfo->next_token.type){
        case LEFT_BRACE:
            Match(LEFT_BRACE);
            currNode = RDParseMathExpr();
            Match(RIGHT_BRACE);
            break;
        case NUM:
            currNode = new TreeNode;
            currNode->expr_data_type = INTEGER;
            currNode->node_kind = NUM_NODE;
            currNode->num = atoi(parseInfo->next_token.str);
            currNode->line_num = compilerInfo->in_file.cur_line_num;
            Match(NUM);
            break;
        case ID:
            currNode = new TreeNode;
            currNode->id = new char[MAX_TOKEN_LEN];
            strcpy(currNode->id, parseInfo->next_token.str);
            currNode->line_num = compilerInfo->in_file.cur_line_num;
            currNode->node_kind = ID_NODE;
            currNode->expr_data_type = INTEGER;
            Match(ID);
            break;
        default:
            char *res = new char[100];
            sprintf(res, "Unexpected expression at line %d", compilerInfo->in_file.cur_line_num);
            PrintError(res);
    }
    return currNode;
}

void ParseTree::Match(const TokenType &token){
    if (parseInfo->next_token.type == token){
        GetNextToken(compilerInfo, &parseInfo->next_token);
    }
    else {
        char *res = new char[100];
        sprintf(res, "Unexpected token [%s] at line %d", parseInfo->next_token.str, compilerInfo->in_file.cur_line_num);
        PrintError(res);
    }
}

#define MAX_FILE_PATH_LENGTH 1000

int main(){
    char* inFile = new char[MAX_FILE_PATH_LENGTH];
    scanf("%s", inFile);

    CompilerInfo compInfo(inFile, "output.txt", "error.txt");
    ParseTree *parseTree = new ParseTree(&compInfo);
    PrintTree(parseTree->root);
    return 0;
}




