/*
 *  Copyright (c) 2019 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 */

#include <pablo/parse/lexer.h>

#include <cctype>
#include <pablo/parse/error_text.h>

namespace pablo {
namespace parse {

inline static bool isTokenSeparator(char c) {
    return !(std::isalnum(c) || c == '_');
}


boost::optional<std::vector<Token *>> Lexer::tokenize(std::shared_ptr<SourceFile> sourceFile) {
    mCurrentSource = sourceFile;
    std::vector<Token *> tokenList{};
    while (mCurrentSource->nextLine(mCurrentLine)) {
        mCurrentColNum = 0;
        mCurrentLineNum++;
        Token * token = nullptr;
        char c;
        while (mCurrentColNum < mCurrentLine.length() && mErrorManager->shouldContinue()) {
            c = mCurrentLine[mCurrentColNum];
            if (std::isspace(c)) {
                mCurrentColNum++;
                continue;
            } else if (c == '#') {
                mCurrentColNum = mCurrentLine.length();
                continue;
            } else if (c == '@') {
                token = extractAttribute();
            } else if (std::isdigit(c)) {
                token = extractIntLiteral();
            } else if (std::isalpha(c) || c == '_') {
                token = extractText();
            } else {
                token = extractSymbol();
            }
            if (token != nullptr) {
                tokenList.push_back(token);
            }
        }
    }

    if (mErrorManager->hasErrors()) {
        return boost::none;
    }
    tokenList.push_back(Token::CreateEOF(mCurrentLineNum == 0 ? 1 : mCurrentLineNum, mCurrentColNum, mCurrentSource));
    return tokenList;
}


Token * Lexer::extractAttribute() {
    const size_t col = mCurrentColNum;
    mCurrentColNum++; // skip '@'
    std::string builder = "@";
    while (mCurrentColNum <= mCurrentLine.length() && std::isalpha(mCurrentLine[mCurrentColNum])) {
        builder.push_back(mCurrentLine[mCurrentColNum]);
        mCurrentColNum++;
    }
    auto token = Token::CreateAttribute(builder, mCurrentLineNum, col, mCurrentSource);
    if (builder == "@") {
        mErrorManager->logError(token, "missing attribute name", "expected a name after '@'");
        return nullptr;
    }
    return token;
}


Token * Lexer::extractText() {
    const size_t col = mCurrentColNum;
    std::string builder{};
    bool canBeIntType = mCurrentLine[mCurrentColNum] == 'i';
    while (mCurrentColNum <= mCurrentLine.length() &&
           (std::isalnum(mCurrentLine[mCurrentColNum]) || mCurrentLine[mCurrentColNum] == '_'))
    {
        const char c = mCurrentLine[mCurrentColNum];
        builder.push_back(c);
        canBeIntType &= std::isdigit(c) || (c == 'i' && mCurrentColNum == col);
        mCurrentColNum++;
    }

    if (canBeIntType && builder.length() > 1) {
        return Token::CreateIntType(std::stoi(builder.substr(1)), mCurrentLineNum, col, mCurrentSource);
    }
    if (builder == "kernel") {
        return Token::CreateKernel(mCurrentLineNum, col, mCurrentSource);
    } else if (builder == "if") {
        return Token::CreateIf(mCurrentLineNum, col, mCurrentSource);
    } else if (builder == "while") {
        return Token::CreateWhile(mCurrentLineNum, col, mCurrentSource);
    } else if (builder == "type") {
        return Token::CreateType(mCurrentLineNum, col, mCurrentSource);
    } else {
        return Token::CreateIdentifier(builder, mCurrentLineNum, col, mCurrentSource);
    }
}


Token * Lexer::extractIntLiteral() {
    const size_t col = mCurrentColNum;
    size_t consumedCount = 0;
    uint64_t value = std::stoul(mCurrentLine.substr(mCurrentColNum).to_string(), &consumedCount, /* auto base */ 0);
    mCurrentColNum += consumedCount;
    if (consumedCount == 0 || (mCurrentColNum < mCurrentLine.length() && !isTokenSeparator(mCurrentLine[mCurrentColNum]))) {
        mErrorManager->logError(mCurrentSource, mCurrentLineNum, col + 1 /*to 1-indexed*/, errtxt_IllegalIntegerLiteral());
        return nullptr;
    }
    std::string text = mCurrentLine.substr(col, consumedCount).to_string();
    return Token::CreateIntLiteral(text, value, mCurrentLineNum, col, mCurrentSource);
}


Token * Lexer::extractSymbol() {
    const size_t col = mCurrentColNum;
    const char c = mCurrentLine[mCurrentColNum];
    Token * token = nullptr;
    #define NEXT_IS(C) \
        mCurrentColNum + 1 < mCurrentLine.length() && mCurrentLine[mCurrentColNum + 1] == C

    switch (c) {
    case '&':
        if (NEXT_IS('=')) {
            token = Token::CreateAndAssign(mCurrentLineNum, col, mCurrentSource);
            mCurrentColNum++;
        } else {
            token = Token::CreateAnd(mCurrentLineNum, col, mCurrentSource);
        }
        break;
    case '=':
        token = Token::CreateAssign(mCurrentLineNum, col, mCurrentSource);
        break;
    case '|':
        if (NEXT_IS('=')) {
            token = Token::CreateOrAssign(mCurrentLineNum, col, mCurrentSource);
            mCurrentColNum++;
        } else {
            token = Token::CreateBar(mCurrentLineNum, col, mCurrentSource);
        }
        break;
    case ',':
        token = Token::CreateComma(mCurrentLineNum, col, mCurrentSource);
        break;
    case '.':
        token = Token::CreateDot(mCurrentLineNum, col, mCurrentSource);
        break;
    case '~':
        token = Token::CreateTilde(mCurrentLineNum, col, mCurrentSource);
        break;
    case '^':
        token = Token::CreateCaret(mCurrentLineNum, col, mCurrentSource);
        break;
    case '(':
        token = Token::CreateLParen(mCurrentLineNum, col, mCurrentSource);
        break;
    case ')':
        token = Token::CreateRParen(mCurrentLineNum, col, mCurrentSource);
        break;
    case '[':
        token = Token::CreateLSBrace(mCurrentLineNum, col, mCurrentSource);
        break;
    case ']':
        token = Token::CreateRSBrace(mCurrentLineNum, col, mCurrentSource);
        break;
    case '{':
        token = Token::CreateLBrace(mCurrentLineNum, col, mCurrentSource);
        break;
    case '}':
        token = Token::CreateRBrace(mCurrentLineNum, col, mCurrentSource);
        break;
    case '<':
        token = Token::CreateLAngle(mCurrentLineNum, col, mCurrentSource);
        break;
    case '>':
        token = Token::CreateRAngle(mCurrentLineNum, col, mCurrentSource);
        break;
    case ':':
        if (NEXT_IS(':')) {
            token = Token::CreateSig(mCurrentLineNum, col, mCurrentSource);
        } else {
            mErrorManager->logError(mCurrentSource, mCurrentLineNum, mCurrentColNum + 1 /*to 1-indexed*/, errtxt_IllegalSymbol(c), "::");
        }
        mCurrentColNum++;
        break;
    case '-':
        if (NEXT_IS('>')) {
            token = Token::CreateArrow(mCurrentLineNum, col, mCurrentSource);
        } else {
            mErrorManager->logError(mCurrentSource, mCurrentLineNum, mCurrentColNum + 1 /*to 1-indexed*/, errtxt_IllegalSymbol(c), "->");
        }
        mCurrentColNum++;
        break;
    default:
        mErrorManager->logError(mCurrentSource, mCurrentLineNum, mCurrentColNum + 1 /*to 1-indexed*/, errtxt_IllegalSymbol(c));
        break;
    }
    mCurrentColNum++;
    return token;
    #undef NEXT_IS
}


Lexer::Lexer(std::shared_ptr<ErrorManager> errorDelegate)
: mErrorManager(std::move(errorDelegate))
, mCurrentSource(nullptr)
, mCurrentLine()
, mCurrentLineNum(0)
, mCurrentColNum(0)
{}

} // namespace pablo::parse
} // namespace pablo
