#pragma once

#include <Util/Util.hpp>

class Tokenizer {
public:
    ENGINE_EXPORT Tokenizer(const std::ifstream& stream, const std::set<char> delims);
    ENGINE_EXPORT Tokenizer(const std::string& buffer , const std::set<char> delims);
    ENGINE_EXPORT ~Tokenizer();

    ENGINE_EXPORT bool Next(std::string& token);
    ENGINE_EXPORT bool Next(float& token);
    ENGINE_EXPORT bool Next(int& token);
    ENGINE_EXPORT bool Next(unsigned int& token);

private:
    char* mBuffer;
    size_t mLength;
    
    std::set<char> mDelimiters;

    size_t mCurrent;
};