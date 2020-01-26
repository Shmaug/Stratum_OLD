#include <Util/Tokenizer.hpp>

#include <sstream>

using namespace std;

Tokenizer::Tokenizer(const ifstream& stream, const set<char> delims) : mDelimiters(delims), mCurrent(0) {
    stringstream ss;
    ss << stream.rdbuf();
    string str = ss.str();
    mBuffer = new char[str.length()];
    memcpy(mBuffer, str.data(), str.length());
    mLength = str.length();
}
Tokenizer::Tokenizer(const string& buffer, const set<char> delims) : mDelimiters(delims), mCurrent(0) {
    mBuffer = new char[buffer.length()];
    memcpy(mBuffer, buffer.data(), buffer.length());
    mLength = buffer.length();
}
Tokenizer::~Tokenizer(){
    delete[] mBuffer;
}

bool Tokenizer::Next(string& token) {
    // eat up any leading delimeters
    while (mCurrent < mLength && mDelimiters.count(mBuffer[mCurrent])) mCurrent++;
    if (mCurrent >= mLength) return false;

    // read until we hit a delimiter
    size_t start = mCurrent;
    while (mCurrent < mLength && !mDelimiters.count(mBuffer[mCurrent])) mCurrent++;
    token = string(mBuffer + start, mCurrent - start);
    return true;
}

bool Tokenizer::Next(float& token) {
    // eat up any leading delimeters
    while (mCurrent < mLength && mDelimiters.count(mBuffer[mCurrent])) mCurrent++;
    if (mCurrent >= mLength) return false;

    // read until we hit a delimiter
    size_t start = mCurrent;
    while (mCurrent < mLength && !mDelimiters.count(mBuffer[mCurrent])) mCurrent++;

    char* buf = new char[1 + mCurrent - start];
    buf[mCurrent - start] = '\0';
    memcpy(buf, mBuffer + start, mCurrent - start);
    token = (float)atof(buf);
    delete[] buf;

    return true;
}

bool Tokenizer::Next(int& token) {
    // eat up any leading delimeters
    while (mCurrent < mLength && mDelimiters.count(mBuffer[mCurrent])) mCurrent++;
    if (mCurrent >= mLength) return false;

    // read until we hit a delimiter
    size_t start = mCurrent;
    while (mCurrent < mLength && !mDelimiters.count(mBuffer[mCurrent])) mCurrent++;
    
    char* buf = new char[1 + mCurrent - start];
    buf[mCurrent - start] = '\0';
    memcpy(buf, mBuffer + start, mCurrent - start);
    token = atoi(buf);
    delete[] buf;

    return true;
}

bool Tokenizer::Next(unsigned int& token) {
    // eat up any leading delimeters
    while (mCurrent < mLength && mDelimiters.count(mBuffer[mCurrent])) mCurrent++;
    if (mCurrent >= mLength) return false;

    // read until we hit a delimiter
    size_t start = mCurrent;
    while (mCurrent < mLength && !mDelimiters.count(mBuffer[mCurrent])) mCurrent++;

    char* buf = new char[1 + mCurrent - start];
    buf[mCurrent - start] = '\0';
    memcpy(buf, mBuffer + start, mCurrent - start);
    token = (uint32_t)atoi(buf);
    delete[] buf;

    return true;
}