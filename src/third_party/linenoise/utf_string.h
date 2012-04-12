template <typename char_type>
struct utfStringMixin {
    typedef char_type char_t; // inherited

    utfStringMixin() : _len(0), _cap(0), _chars(0) {}

    utfStringMixin(const utfStringMixin& other) // copies like std::string
        :_len(other._len), _cap(other._len+1), _chars(other._chars), _str(new char_t[_cap])
    {
        memcpy(_str.get(), other._str.get(), _cap);
    }

    utfStringMixin& operator= (utfStringMixin copy) {
        this->swap(copy);
        return *this;
    }

    /// After construction this returns false if we failed to convert
    bool isOk() const { return bool(_str); }

    char_t* get() { return _str.get(); }
    char_t& operator[](size_t idx) { return _str[idx]; }

    const char_t* get() const { return _str.get(); }
    const char_t& operator[](size_t idx) const { return _str[idx]; }

    size_t len() const { return _len; }
    size_t cap() const { return _cap; }
    size_t chars() const { return _chars; }

    void swap(utfStringMixin& other) {
        std::swap(_len, other._len);
        std::swap(_cap, other._cap);
        std::swap(_chars, other._chars);
        _str.swap(other._str);
    }

    protected:
    size_t _len; // in units of char_t w/o nul
    size_t _cap; // size of _str buffer inc nul
    size_t _chars; // number of codepoints
    boost::scoped_array<char_t> _str;
};

struct utf32String;

struct utf8String : public utfStringMixin<UChar8> {
    utf8String() {}
    explicit utf8String(const UChar32* s, ssize_t chars=-1) {
        if (chars == -1)
            initFrom32(s, strlen32(s));
        else
            initFrom32(s, chars);
    }
    explicit utf8String(const utf32String& c); // defined after utf32String

    // note: these are not on utr32String
    char* c_str() { return reinterpret_cast<char*>(get()); }
    const char* c_str() const { return reinterpret_cast<const char*>(get()); }

private:
    void initFrom32(const UChar32* s, size_t chars) {
        _chars = chars;
        _cap = _chars*4 + 1;
        _str.reset(new char_t[_cap]);
        _len = copyString32to8counted(_str.get(), s, _cap, chars);
    }
};

struct utf32String : public utfStringMixin<UChar32> {
    utf32String() {}
    explicit utf32String(const UChar8* s, ssize_t chars=-1) {
        initFrom8(s, chars);
    }
    explicit utf32String(const utf8String& c){
        initFrom8(c.get(), c.chars());
    }


private:
    void initFrom8(const UChar8* s, size_t chars) {
        utf32String temp; // swapped with *this if conversion is successful
        if (chars == -1)
            temp._cap = strlen(reinterpret_cast<const char*>(s)) + 1; // worst case ASCII
        else
            temp._cap = chars + 1;

        temp._str.reset(new char_t[temp._cap]);

        int error;
        copyString8to32(temp._str.get(), s, temp._cap, temp._chars, error);

        if (!error){
            temp._len = temp._chars; // this is utf32
            this->swap(temp);
        }
    }
};

inline utf8String:: utf8String(const utf32String& s) {
    initFrom32(s.get(), s.chars());
}
