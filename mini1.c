#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/*
For the language grammar, please refer to Grammar section on the github page:
  https://github.com/lightbulb12294/CSI2P-II-Mini1#grammar
*/

#define MAX_LENGTH 200
typedef enum {
    ASSIGN, ADD, SUB, MUL, DIV, REM, PREINC, PREDEC, POSTINC, POSTDEC, IDENTIFIER, CONSTANT, LPAR, RPAR, PLUS, MINUS, END
} Kind;
typedef enum {
    STMT, EXPR, ASSIGN_EXPR, ADD_EXPR, MUL_EXPR, UNARY_EXPR, POSTFIX_EXPR, PRI_EXPR
} GrammarState;
typedef struct TokenUnit {
    Kind kind;
    int val; // record the integer value or variable name
    struct TokenUnit *next;
} Token;
typedef struct ASTUnit {
    Kind kind;
    int val; // record the integer value or variable name(ASCII number) 
    struct ASTUnit *lhs, *mid, *rhs;
} AST;

/// utility interfaces

// err marco should be used when a expression error occurs.
#define err(x) {\
    puts("Compile Error!");\
    if(DEBUG) {\
        fprintf(stderr, "Error at line: %d\n", __LINE__);\
        fprintf(stderr, "Error message: %s\n", x);\
    }\
    exit(0);\
}
// You may set DEBUG=1 to debug. Remember setting back to 0 before submit.
#define DEBUG 0
// Split the input char array into token linked list.
Token *lexer(const char *in);
// Create a new token.
Token *new_token(Kind kind, int val);
// Translate a token linked list into array, return its length.
size_t token_list_to_arr(Token **head);
// Parse the token array. Return the constructed AST.
AST *parser(Token *arr, size_t len);
// Parse the token array. Return the constructed AST.
AST *parse(Token *arr, int l, int r, GrammarState S);
// Create a new AST node.
AST *new_AST(Kind kind, int val);
// Find the location of next token that fits the condition(cond). Return -1 if not found. Search direction from start to end.
int findNextSection(Token *arr, int start, int end, int (*cond)(Kind));
// Return 1 if kind is ASSIGN.
int condASSIGN(Kind kind);
// Return 1 if kind is ADD or SUB.
int condADD(Kind kind);
// Return 1 if kind is MUL, DIV, or REM.
int condMUL(Kind kind);
// Return 1 if kind is RPAR.
int condRPAR(Kind kind);
// Check if the AST is semantically right. This function will call err() automatically if check failed.
void semantic_check(AST *now);
// Generate ASM code.
void codegen(AST *root);
// Free the whole AST.
void freeAST(AST *now);

/// debug interfaces

// Print token array.
void token_print(Token *in, size_t len);
// Print AST tree.
void AST_print(AST *head);

char input[MAX_LENGTH];

int main() {
    while (fgets(input, MAX_LENGTH, stdin) != NULL) {
        Token *content = lexer(input); 
        size_t len = token_list_to_arr(&content);
        if (len == 0) continue;
        AST *ast_root = parser(content, len);
        //debug
        //token_print(content, len);
        //AST_print(ast_root);
        //debug
        semantic_check(ast_root);
        codegen(ast_root);
        free(content);
        freeAST(ast_root);
    }
    return 0;
}

Token *lexer(const char *in) {  //convert input into Token linked list
    Token *head = NULL;
    Token **now = &head;
    for (int i = 0; in[i]; i++) {
        if (isspace(in[i])) // ignore space characters
            continue;
        else if (isdigit(in[i])) {
            (*now) = new_token(CONSTANT, atoi(in + i)); //atoi:convert str to int
            while (in[i+1] && isdigit(in[i+1])) i++; //update i to the last of valid char
        }
        else if ('x' <= in[i] && in[i] <= 'z') // variable
            (*now) = new_token(IDENTIFIER, in[i]);
        else switch (in[i]) {
            case '=':
                (*now) = new_token(ASSIGN, 0);
                break;
            case '+':
                if (in[i+1] && in[i+1] == '+') {
                    i++;
                    // In lexer scope, all "++" will be labeled as PREINC.
                    (*now) = new_token(PREINC, 0);
                }
                // In lexer scope, all single "+" will be labeled as PLUS.
                else (*now) = new_token(PLUS, 0);
                break;
            case '-':
                if (in[i+1] && in[i+1] == '-') {
                    i++;
                    // In lexer scope, all "--" will be labeled as PREDEC.
                    (*now) = new_token(PREDEC, 0);
                }
                // In lexer scope, all single "-" will be labeled as MINUS.
                else (*now) = new_token(MINUS, 0);
                break;
            case '*':
                (*now) = new_token(MUL, 0);
                break;
            case '/':
                (*now) = new_token(DIV, 0);
                break;
            case '%':
                (*now) = new_token(REM, 0);
                break;
            case '(':
                (*now) = new_token(LPAR, 0);
                break;
            case ')':
                (*now) = new_token(RPAR, 0);
                break;
            case ';':
                (*now) = new_token(END, 0);
                break;
            default:
                err("Unexpected character.");
        }
        now = &((*now)->next);
    }
    return head;
}

Token *new_token(Kind kind, int val) {
    Token *res = (Token*)malloc(sizeof(Token));
    res->kind = kind;
    res->val = val;
    res->next = NULL;
    return res;
}

size_t token_list_to_arr(Token **head) { //pass &content into this func
    size_t res;
    Token *now = (*head), *del;
    for (res = 0; now != NULL; res++) now = now->next; //calulate the length
    now = (*head);
    if (res != 0) (*head) = (Token*)malloc(sizeof(Token) * res);
    for (int i = 0; i < res; i++) {
        (*head)[i] = (*now);
        del = now;
        now = now->next;
        free(del);
    }
    return res;
}

AST *parser(Token *arr, size_t len) { //解決lexer無法辨別PLUS/ADD,MINUS/SUB
    for (int i = 1; i < len; i++) {
        // correctly identify "ADD" and "SUB"
        if (arr[i].kind == PLUS || arr[i].kind == MINUS) {
            switch (arr[i - 1].kind) {
                case PREINC:
                case PREDEC:
                case IDENTIFIER:
                case CONSTANT:
                case RPAR:
                    arr[i].kind = arr[i].kind - PLUS + ADD; //符合上述的kind辨別成ADD or SUB
                default: break;
            }
        }
    }
    return parse(arr, 0, len - 1, STMT);
}

AST *parse(Token *arr, int l, int r, GrammarState S) { //already filter some error about grammar
    AST *now = NULL;
    if (l > r)
        err("Unexpected parsing range.");
    int nxt;
    switch (S) {
        case STMT:
            if (l == r && arr[l].kind == END) //expression only contains ';'
                return NULL;
            else if (arr[r].kind == END)
                return parse(arr, l, r - 1, EXPR);
            else err("Expected \';\' at the end of line.");
        case EXPR:
            return parse(arr, l, r, ASSIGN_EXPR);
        case ASSIGN_EXPR:
            if ((nxt = findNextSection(arr, l, r, condASSIGN)) != -1) { //find assign sign
                now = new_AST(arr[nxt].kind, 0);
                now->lhs = parse(arr, l, nxt - 1, UNARY_EXPR);
                now->rhs = parse(arr, nxt + 1, r, ASSIGN_EXPR);
                return now;
            }
            return parse(arr, l, r, ADD_EXPR);
        case ADD_EXPR:
            if((nxt = findNextSection(arr, r, l, condADD)) != -1) {
                //finNxt from right to left
                now = new_AST(arr[nxt].kind, 0);
                now->lhs = parse(arr, l, nxt - 1, ADD_EXPR);
                now->rhs = parse(arr, nxt + 1, r, MUL_EXPR);
                return now;
            }
            return parse(arr, l, r, MUL_EXPR);
        case MUL_EXPR:
            // TODO: Implement MUL_EXPR.
            // hint: Take ADD_EXPR as reference.
            if((nxt = findNextSection(arr, r, l, condMUL)) != -1) {
                now = new_AST(arr[nxt].kind, 0);
                now->lhs = parse(arr, l, nxt - 1, MUL_EXPR);
                now->rhs = parse(arr, nxt + 1, r, UNARY_EXPR);
                return now;
            }
            return parse(arr, l, r, UNARY_EXPR);
        case UNARY_EXPR:
            // TODO: Implement UNARY_EXPR.
            // hint: Take POSTFIX_EXPR as reference.
            if (arr[l].kind == PREINC || arr[l].kind == PREDEC|| arr[l].kind == PLUS || arr[l].kind == MINUS) {
                now = new_AST(arr[l].kind , 0);
                now->mid = parse(arr, l + 1, r , UNARY_EXPR);
                return now;
            }
            return parse(arr, l, r, POSTFIX_EXPR);
        case POSTFIX_EXPR:
            if (arr[r].kind == PREINC || arr[r].kind == PREDEC) {
                // translate "PREINC", "PREDEC" into "POSTINC", "POSTDEC"
                now = new_AST((arr[r].kind - PREINC + POSTINC), 0);
                now->mid = parse(arr, l, r - 1, POSTFIX_EXPR);
                return now;
            }
            return parse(arr, l, r, PRI_EXPR);
        case PRI_EXPR:
            if (findNextSection(arr, l, r, condRPAR) == r) { //satisfy LPAR EXPR RPAR
                now = new_AST(LPAR, 0); //RPAR didn't construct node
                now->mid = parse(arr, l + 1, r - 1, EXPR); //LPAR->mid=EXPR
                return now;
            }
            if (l == r) {
                if (arr[l].kind == IDENTIFIER || arr[l].kind == CONSTANT)
                    return new_AST(arr[l].kind, arr[l].val);
                err("Unexpected token during parsing.");
            }
            err("No token left for parsing.");
        default:
            err("Unexpected grammar state.");
    }
}

AST *new_AST(Kind kind, int val) {
    AST *res = (AST*)malloc(sizeof(AST));
    res->kind = kind;
    res->val = val;
    res->lhs = res->mid = res->rhs = NULL;
    return res;
}

int findNextSection(Token *arr, int start, int end, int (*cond)(Kind)) {
    int par = 0;
    int d = (start < end) ? 1 : -1;
    for (int i = start; (start < end) ? (i <= end) : (i >= end); i += d) {
        if (arr[i].kind == LPAR) par++;
        if (arr[i].kind == RPAR) par--;
        if (par == 0 && cond(arr[i].kind) == 1) return i;
    }
    return -1; //doesn't satisfy
}

int condASSIGN(Kind kind) {
    return kind == ASSIGN;
}

int condADD(Kind kind) {
    return kind == ADD || kind == SUB;
}

int condMUL(Kind kind) {
    return kind == MUL || kind == DIV || kind == REM;
}

int condRPAR(Kind kind) {
    return kind == RPAR;
}

void semantic_check(AST *now) {  //invalid logic  (go through postorder traversal (lefft right mid))
    if (now == NULL) return;
    // Left operand of '=' must be an identifier or identifier with one or more parentheses.
    if (now->kind == ASSIGN) {
        AST *tmp = now->lhs;
        while (tmp->kind == LPAR) tmp = tmp->mid; // LPAR->mid = EXPR
        if (tmp->kind != IDENTIFIER)
            err("Lvalue is required as left operand of assignment.");
    }
    // Operand of INC/DEC must be an identifier or identifier with one or more parentheses.
    // TODO: Implement the remaining semantic_check code.
    // hint: Follow the instruction above and ASSIGN-part code to implement.
    // hint: Semantic of each node needs to be checked recursively (from the current node to lhs/mid/rhs node).
    if(now->kind == PREDEC || now->kind == PREINC || now->kind == POSTINC || now->kind == POSTDEC){
        AST *tmp = now->mid;
        while (tmp->kind == LPAR) tmp = tmp->mid;
        if (tmp->kind != IDENTIFIER)
            err("Identifier is required as mid operand of INC/DEC.");
    }
    semantic_check(now->lhs);
    semantic_check(now->rhs);
    semantic_check(now->mid);
}

void codegen(AST *root) {
    // TODO: Implement your codegen in your own way.
    // You may modify the function parameter or the return type, even the whole structure as you wish.
    //new idea: post-order traversal or write ASM structure ?

    const static char MemOP[][6]={"load","store"};
    const static char ArithOP[][5]={"add","sub","mul","div","rem"};
    const static int memAd[3]={0,4,8}; //x:[0],y:[4],z:[8]
    static int regIdx;
    static int memJudge;
    static int Val;
    
    if (root == NULL) return;
    switch(root->kind){
        case ASSIGN:
            regIdx++;
            codegen(root->rhs);
            regIdx--;
            
            printf("add r%d 0 r%d\n",regIdx,regIdx+1); //update to up level
            
            memJudge++; //left of assign must do store
            codegen(root->lhs);
            memJudge--;
            break;
        case ADD:
        case SUB:
        case MUL:
        case DIV:
        case REM:
            codegen(root->lhs);
            
            regIdx++;
            codegen(root->rhs);
            regIdx--;
            //ArithOP[(root->kind)-ADD]:match the content in ArithOP
            printf("%s r%d r%d r%d\n",ArithOP[(root->kind)-ADD],regIdx,regIdx,regIdx+1);
            break;

        case POSTINC: //operate before adding
        case POSTDEC:
            codegen(root->mid);//load
            printf("%s r%d r%d 1\n",ArithOP[(root->kind)-POSTINC],regIdx+1,regIdx);
            printf("%s [%d] r%d\n",MemOP[1],memAd[Val-'x'],regIdx+1); //store
            
            break;
        case PREINC:
        case PREDEC: //operate after adding
            codegen(root->mid);//load
            printf("%s r%d r%d 1\n",ArithOP[(root->kind)-PREINC],regIdx,regIdx);
            printf("%s [%d] r%d\n",MemOP[1],memAd[Val-'x'],regIdx); //store
            
            break;
        case PLUS:
        case MINUS:
            codegen(root->mid);
            printf("%s r%d 0 r%d\n",ArithOP[(root->kind)-PLUS],regIdx,regIdx);
            break;
        case LPAR:
            codegen(root->mid);
            break;
        case IDENTIFIER:
            Val = root->val;
            if(memJudge==1)
                printf("%s [%d] r%d\n",MemOP[memJudge],memAd[(root->val)-'x'],regIdx);
            else
                printf("%s r%d [%d]\n",MemOP[memJudge],regIdx,memAd[(root->val)-'x']);
            break;
        case CONSTANT:
            printf("add r%d 0 %d\n",regIdx,root->val);
            break;
            
    }
    
}


void freeAST(AST *now) {
    if (now == NULL) return;
    freeAST(now->lhs);
    freeAST(now->mid);
    freeAST(now->rhs);
    free(now);
}

void token_print(Token *in, size_t len) {
    const static char KindName[][20] = {
        "Assign", "Add", "Sub", "Mul", "Div", "Rem", "Inc", "Dec", "Inc", "Dec", "Identifier", "Constant", "LPar", "RPar", "Plus", "Minus", "End"
    };
    const static char KindSymbol[][20] = {
        "'='", "'+'", "'-'", "'*'", "'/'", "'%'", "\"++\"", "\"--\"", "\"++\"", "\"--\"", "", "", "'('", "')'", "'+'", "'-'"
    };
    const static char format_str[] = "<Index = %3d>: %-10s, %-6s = %s\n";
    const static char format_int[] = "<Index = %3d>: %-10s, %-6s = %d\n";
    for(int i = 0; i < len; i++) {
        switch(in[i].kind) {
            case LPAR:
            case RPAR:
            case PREINC:
            case PREDEC:
            case ADD:
            case SUB:
            case MUL:
            case DIV:
            case REM:
            case ASSIGN:
            case PLUS:
            case MINUS:
                printf(format_str, i, KindName[in[i].kind], "symbol", KindSymbol[in[i].kind]);
                break;
            case CONSTANT:
                printf(format_int, i, KindName[in[i].kind], "value", in[i].val);
                break;
            case IDENTIFIER:
                printf(format_str, i, KindName[in[i].kind], "name", (char*)(&(in[i].val)));
                break;
            case END:
                printf("<Index = %3d>: %-10s\n", i, KindName[in[i].kind]);
                break;
            default:
                puts("=== unknown token ===");
        }
    }
}

void AST_print(AST *head) {
    /*
    static char indent_str[MAX_LENGTH] = "";
    static int indent = 0;
    char *indent_now = indent_str + indent;
    */
    // new version
    static char indent_str[MAX_LENGTH] = "  ";
    static int indent = 2;
    //
    const static char KindName[][20] = {
        "Assign", "Add", "Sub", "Mul", "Div", "Rem", "PreInc", "PreDec", "PostInc", "PostDec", "Identifier", "Constant", "Parentheses", "Parentheses", "Plus", "Minus"
    };
    const static char format[] = "%s\n";
    const static char format_str[] = "%s, <%s = %s>\n";
    const static char format_val[] = "%s, <%s = %d>\n";
    if (head == NULL) return;
    //if (indent!=0){
        char *indent_now = indent_str + indent; //new version
        indent_str[indent - 1] = '-';
        printf("%s", indent_str);
        indent_str[indent - 1] = ' ';
        if (indent_str[indent - 2] == '`')
            indent_str[indent - 2] = ' ';
    //}
    
    switch (head->kind) {
        case ASSIGN:
        case ADD:
        case SUB:
        case MUL:
        case DIV:
        case REM:
        case PREINC:
        case PREDEC:
        case POSTINC:
        case POSTDEC:
        case LPAR:
        case RPAR:
        case PLUS:
        case MINUS:
            printf(format, KindName[head->kind]);
            break;
        case IDENTIFIER:
            printf(format_str, KindName[head->kind], "name", (char*)&(head->val)); //format_str="%s, <%s = %s>\n";
            break;
        case CONSTANT:
            printf(format_val, KindName[head->kind], "value", head->val); //format_val="%s, <%s = %d>\n";
            break;
        default:
            puts("=== unknown AST type ===");
    }
    indent += 2;
    strcpy(indent_now, "| ");
    AST_print(head->lhs);
    strcpy(indent_now, "` ");
    AST_print(head->mid);
    AST_print(head->rhs);
    indent -= 2;
    (*indent_now) = '\0';
}


