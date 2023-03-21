///////////////////////////////////////////////////////////////////////////////
// Name:        html_extract_text.cpp
// Author:      Blake Madden
// Copyright:   (c) 2005-2022 Blake Madden
// Licence:     3-Clause BSD licence
// SPDX-License-Identifier: BSD-3-Clause
///////////////////////////////////////////////////////////////////////////////

#include "html_extract_text.h"

using namespace string_util;

namespace lily_of_the_valley
    {
    //------------------------------------------------------------------
    std::wstring_view html_extract_text::read_element_as_string(const wchar_t* html_text,
                                                           const wchar_t* html_end,
                                                           const wchar_t* element,
                                                           const size_t element_length)
        {
        assert(html_text && html_end && element && element_length > 0);
        const wchar_t* elementStart = find_element(html_text, html_end, element, element_length);
        if (elementStart)
            {
            const wchar_t* elementEnd = find_closing_element(elementStart, html_end, element, element_length);
            elementStart = find_close_tag(elementStart);
            if (elementStart && elementEnd)
                {
                ++elementStart;
                return string_util::trim_view(std::wstring_view(elementStart, elementEnd - elementStart));
                }
            }
        return std::wstring_view{};
        }

    //------------------------------------------------------------------
    std::wstring html_extract_text::read_attribute_as_string(const wchar_t* text,
            const wchar_t* attribute, const size_t attributeSize,
            const bool allowQuotedTags,
            const bool allowSpacesInValue /*= false*/)
        {
        if (!text || !attribute || *attribute == 0)
            { return L""; }
        assert((std::wcslen(attribute) == attributeSize) &&
               "Invalid length passed to read_tag_as_string().");
        const auto rt = read_attribute(text, attribute, attributeSize, allowQuotedTags, allowSpacesInValue);
        if (rt.first == nullptr)
            { return L""; }
        else
            { return std::wstring(rt.first,rt.second); }
        }

    //------------------------------------------------------------------
    long html_extract_text::read_attribute_as_long(const wchar_t* text,
            const wchar_t* attribute, const size_t attributeSize,
            const bool allowQuotedTags)
        {
        const std::wstring tagStr = read_attribute_as_string(text, attribute, attributeSize, allowQuotedTags, false);
        wchar_t* dummy{ nullptr };
        return tagStr.length() ? std::wcstol(tagStr.c_str(),&dummy,10) : 0;
        }

    //------------------------------------------------------------------
    const std::pair<const wchar_t*,std::wstring> html_extract_text::find_bookmark(
                                            const wchar_t* sectionStart,
                                            const wchar_t* sectionEnd)
        {
        const wchar_t* nextAnchor = find_element(sectionStart, sectionEnd, L"a", 1);
        if (nextAnchor)
            {
            auto [bookMarkPtr, bookMarkSize] = read_attribute(nextAnchor, L"name", 4, false);
            if (bookMarkPtr && bookMarkSize > 0)
                {
                // chop if the leading '#' from the bookmark name
                if (bookMarkPtr[0] == L'#')
                    {
                    ++bookMarkPtr;
                    --bookMarkSize;
                    }
                return std::make_pair(nextAnchor, std::wstring(bookMarkPtr, bookMarkSize));
                }
            // if this anchor doesn't have a bookmark, then recursively look for the next candidate
            return find_bookmark(nextAnchor+1, sectionEnd);
            }
        return std::make_pair(nullptr, L"");
        }

    //------------------------------------------------------------------
    const wchar_t* html_extract_text::strchr_not_quoted(const wchar_t* string, const wchar_t ch) noexcept
        {
        if (!string)
            { return nullptr; }
        bool is_inside_of_quotes = false;
        bool is_inside_of_single_quotes = false;
        while (string)
            {
            if (string[0] == 0)
                { return nullptr; }
            else if (string[0] == 0x22)//double quote
                {
                is_inside_of_quotes = !is_inside_of_quotes;
                // whether this double quote ends a quote pair or starts a new one, turn this flag
                // off. This means that a double quote can close a single quote.
                is_inside_of_single_quotes = false;
                }
            // if a single quote already started a quote pair (and this is closing it) or
            // we are not inside of a double quote then count single quotes
            else if ((!is_inside_of_quotes || is_inside_of_single_quotes) && string[0] == 0x27) // single quote
                {
                is_inside_of_quotes = !is_inside_of_quotes;
                is_inside_of_single_quotes = true;
                }
            if (!is_inside_of_quotes && string[0] == ch)
                { return string; }
            ++string;
            }
        return nullptr;
        }

    //------------------------------------------------------------------
    void html_extract_text::parse_raw_text(const wchar_t* text, size_t textSize)
        {
        if (textSize > 0)
            {
            size_t currentStartPosition{ 0 };
            while (textSize > 0)
                {
                size_t index{ 0 };
                // if preformatted then just look for ampersands or template placeholders
                if (m_is_in_preformatted_text_block_stack > 0)
                    { index = string_util::strncspn<wchar_t>(text+currentStartPosition, textSize, L"&$", 2); }
                // otherwise, eat up crlfs and replace with spaces
                else
                    { index = string_util::strncspn<wchar_t>(text+currentStartPosition, textSize, L"\r\n&$", 4); }
                index += currentStartPosition; // move the index to the position in the full valid string
                if (index < textSize)
                    {
                    if (text[index] == L'&')
                        {
                        const wchar_t* semicolon = string_util::strcspn_pointer<wchar_t>(text+index+1, L";< \t\n\r", 6);
                        /* this should not happen in valid HTML, but in case there is an
                           orphan '&' then skip it and look for the next item.*/
                        if (semicolon == nullptr ||
                            (semicolon > text+textSize))
                            {
                            // copy over the proceeding text (up to the ampersand)
                            if (index > 0)
                                {
                                add_characters(text, index);
                                add_character(L'&');
                                }
                            // update indices into the raw HTML text
                            if ((index+1) > textSize)
                                { textSize = 0; }
                            else
                                { textSize -= index+1; }
                            text += index+1;
                            currentStartPosition = 0;
                            continue;
                            }
                        else
                            {
                            // copy over the proceeding text
                            if (index > 0)
                                { add_characters(text, index); }
                            // in case this is an unencoded ampersand then treat it as such
                            if (std::iswspace(text[index+1]))
                                {
                                add_character(L'&');
                                add_character(L' ');
                                }
                            // convert an encoded number to character
                            else if (text[index+1] == L'#')
                                {
                                size_t hexLength(textSize-(index+3));
                                const wchar_t value = is_either(text[index+2], L'x', L'X') ?
                                    // if it is hex encoded
                                    static_cast<wchar_t>(string_util::axtoi(text+index+3, hexLength))/*skip "&#x"*/ :
                                    // else it is a plain numeric value
                                    static_cast<wchar_t>(string_util::atoi(text+index+2));
                                if (value != 173) // soft hyphens should just be stripped out
                                    {
                                    // ligatures
                                    if (value >= 0xFB00 && value <= 0xFB06)
                                        {
                                        switch (value)
                                            {
                                        case 0xFB00:
                                            add_characters(L"ff",2);
                                            break;
                                        case 0xFB01:
                                            add_characters(L"fi",2);
                                            break;
                                        case 0xFB02:
                                            add_characters(L"fl",2);
                                            break;
                                        case 0xFB03:
                                            add_characters(L"ffi",3);
                                            break;
                                        case 0xFB04:
                                            add_characters(L"ffl",3);
                                            break;
                                        case 0xFB05:
                                            add_characters(L"ft",2);
                                            break;
                                        case 0xFB06:
                                            add_characters(L"st",2);
                                            break;
                                            };
                                        }
                                    else if (value != 0)
                                        { add_character(value); }
                                    // in case conversion failed to come up with a number
                                    // (incorrect encoding in the HTML maybe)
                                    else
                                        {
                                        log_message(L"Invalid numeric HTML entity: "+std::wstring(text+index, (semicolon+1)-(text+index)));
                                        add_characters(text+index, (semicolon+1)-(text+index));
                                        }
                                    }
                                }
                            // look up named entities, such as "amp" or "nbsp"
                            else
                                {
                                const wchar_t value = HTML_TABLE_LOOKUP.find(text+index+1, semicolon-(text+index+1));
                                if (value != 173) // soft hyphens should just be stripped out
                                    {
                                    // Missing semicolon and not a valid entity?
                                    // Must be an unencoded ampersand with a letter right next to it, so just copy that over.
                                    if (value == L'?' && semicolon[0] != L';')
                                        {
                                        log_message(L"Unencoded ampersand or unknown HTML entity: "+std::wstring(text+index, semicolon-(text+index)));
                                        add_characters(text+index, (semicolon-(text+index)+1) );
                                        }
                                    else
                                        {
                                        // Check for something like "&amp;le;", which should really be "&le;".
                                        // Workaround around it and log a warning.
                                        bool leadingAmpersandEncodedCorrectly = true;
                                        if (semicolon[0] == L';' &&
                                            value == L'&')
                                            {
                                            const wchar_t* nextTerminator = semicolon+1;
                                            while (!iswspace(*nextTerminator) && *nextTerminator != L';' &&
                                                    nextTerminator < (text+textSize))
                                                { ++nextTerminator; }
                                            if (nextTerminator < (text+textSize) && *nextTerminator == L';')
                                                {
                                                const wchar_t badlyEncodedEntity = HTML_TABLE_LOOKUP.find(semicolon+1, nextTerminator-(semicolon+1));
                                                if (badlyEncodedEntity != L'?')
                                                    {
                                                    log_message(L"Ampersand incorrectly encoded in HTML entity: "+std::wstring(text+index, (nextTerminator-(text+index))+1));
                                                    leadingAmpersandEncodedCorrectly = false;
                                                    semicolon = nextTerminator;
                                                    add_character(badlyEncodedEntity);
                                                    }
                                                }
                                            }
                                        // appears to be a correctly-formed entity
                                        if (leadingAmpersandEncodedCorrectly)
                                            {
                                            add_character(value);
                                            if (value == L'?')
                                                { log_message(L"Unknown HTML entity: "+std::wstring(text+index, semicolon-(text+index))); }
                                            // Entity not correctly terminated by a semicolon.
                                            // Here we will copy over the converted entity and trailing character
                                            // (a space or newline).
                                            if (semicolon[0] != L';')
                                                {
                                                log_message(L"Missing semicolon on HTML entity: "+std::wstring(text+index, semicolon-(text+index)));
                                                add_character(semicolon[0]);
                                                }
                                            }
                                        }
                                    }
                                }
                            // update indices into the raw HTML text
                            if (static_cast<size_t>((semicolon+1)-(text)) > textSize)
                                { textSize = 0; }
                            else
                                { textSize -= (semicolon+1)-(text); }
                            text = semicolon+1;
                            currentStartPosition = 0;
                            }
                        }
                    // JS template placeholders ${}
                    else if (text[index] == L'$')
                        {
                        const wchar_t* closingBrace{ nullptr };
                        // if a ${, then look for the closing }
                        if (index+1 < textSize &&
                            text[index+1] == L'{')
                            { closingBrace = string_util::strnchr(text+1, L'}', textSize); }
                        // either not closed, or a regular $
                        if (closingBrace == nullptr ||
                            (closingBrace > text+textSize))
                            {
                            // copy over the proceeding text (up to the $)
                            if (index > 0)
                                { add_characters(text, index); }
                            // add the $
                            add_character(L'$');
                            // update indices into the raw HTML text
                            if ((index+1) > textSize)
                                { textSize = 0; }
                            else
                                { textSize -= index+1; }
                            text += index+1;
                            currentStartPosition = 0;
                            continue;
                            }
                        else
                            {
                            // copy over the proceeding text (before the placeholder)
                            if (index > 0)
                                { add_characters(text, index); }
                            // step over the placeholder
                            if (static_cast<size_t>((closingBrace+1)-(text)) > textSize)
                                { textSize = 0; }
                            else
                                { textSize -= (closingBrace+1)-(text); }
                            text = closingBrace+1;
                            currentStartPosition = 0;
                            continue;
                            }
                        }
                    else
                        {
                        // copy over the proceeding text
                        if (m_superscript_stack > 0)
                            {
                            // convert what we can along the way
                            for (size_t i = 0; i < index; ++i)
                                { add_character(string_util::to_superscript(text[i])); }
                            }
                        else if (m_subscript_stack > 0)
                            {
                            // convert what we can along the way
                            for (size_t i = 0; i < index; ++i)
                                { add_character(string_util::to_subscript(text[i])); }
                            }
                        else
                            { add_characters(text, index); }
                        
                        add_character(L' ');
                        // update indices into the raw HTML text
                        if ((index+1) > textSize)
                            { textSize = 0; }
                        else
                            { textSize -= index+1; }
                        text += index+1;
                        currentStartPosition = 0;
                        }
                    }
                // didn't find anything else, so stop scanning this section of text
                else
                    { break; }
                }

            if (textSize > 0)
                {
                if (m_superscript_stack > 0)
                    {
                    // convert what we can along the way
                    for (size_t i = 0; i < textSize; ++i)
                        { add_character(string_util::to_superscript(text[i])); }
                    }
                else if (m_subscript_stack > 0)
                    {
                    // convert what we can along the way
                    for (size_t i = 0; i < textSize; ++i)
                        { add_character(string_util::to_subscript(text[i])); }
                    }
                else
                    { add_characters(text, textSize); }
                }
            }
        }

    //------------------------------------------------------------------
    std::wstring html_extract_text::convert_symbol_font_section(const std::wstring_view& symbolFontText)
        {
        std::wstring convertedText;
        convertedText.reserve(symbolFontText.length());
        for (size_t i = 0; i < symbolFontText.length(); ++i)
            { convertedText += SYMBOL_FONT_TABLE.find(symbolFontText[i]); }
        return convertedText;
        }

    //------------------------------------------------------------------
    std::string html_extract_text::parse_charset(const char* pageContent, const size_t length)
        {
        std::string charset;
        if (pageContent == nullptr || pageContent[0] == 0)
            { return charset; }

        const char* const end = pageContent+length;
        const char* start = string_util::strnistr(pageContent, "<meta", (end-pageContent));
        // No Meta section?
        if (!start)
            {
            // See if this XML and parse it that way. Otherwise, there is no charset.
            if (std::strncmp(pageContent, "<?xml", 5) == 0)
                {
                const char* encoding = std::strstr(pageContent, "encoding=\"");
                if (encoding)
                    {
                    encoding += 10;
                    const char* encodingEnd = std::strchr(encoding, '\"');
                    if (encodingEnd)
                        { charset = std::string(encoding, (encodingEnd-encoding)); }
                    }
                }
            return charset;
            }
        while (start < end)
            {
            const char* nextAngleSymbol = string_util::strnchr(start, '>', (end-start));
            const char* contentType = string_util::strnistr(start, "content-type", (end-start));
            if (!contentType || !nextAngleSymbol)
                { return charset; }
            const char* contentStart = string_util::strnistr(start, " content=", (end-start));
            if (!contentStart)
                { return charset; }
            // if the content-type and content= are inside of this meta tag then
            // it's legit, so move to it and stop looking
            if (contentType < nextAngleSymbol &&
                contentStart < nextAngleSymbol)
                {
                start = contentStart;
                break;
                }
            // otherwise, skip to the next meta tag
            start = string_util::strnistr(nextAngleSymbol, "<meta", (end-nextAngleSymbol));
            if (!start)
                { return charset; }
            }

        start += 9;
        if (start[0] == '\"' || start[0] == '\'')
            { ++start; }
        const char* nextAngle = string_util::strnchr(start, '>', (end-start));
        const char* nextClosedAngle = string_util::strnistr(start, "/>", (end-start));
        // no close angle?  This HMTL is messed up, so just return the default charset
        if (!nextAngle && !nextClosedAngle)
            { return charset; }
        // see which closing angle is the closest one
        if (nextAngle && nextClosedAngle)
            { nextAngle = std::min(nextAngle,nextClosedAngle); }
        else if (!nextAngle && nextClosedAngle)
            { nextAngle = nextClosedAngle; }

        // find and parse the content type
        bool charsetFound = false;
        const char* const contentSection = start;
        start = string_util::strnistr(contentSection, "charset=", (end-contentSection));
        if (start && start < nextAngle)
            {
            start += 8;
            charsetFound = true;
            }
        else
            {
            start = string_util::strnchr<char>(contentSection, L';', (end-contentSection));
            if (start && start < nextAngle)
                {
                ++start;
                charsetFound = true;
                }
            }
        if (start && charsetFound)
            {
            // chop off any quotes and trailing whitespace
            while (start < nextAngle)
                {
                if (start[0] == L' ' || start[0] == '\'')
                    { ++start; }
                else
                    { break; }
                }
            const char* charsetEnd = start;
            while (charsetEnd < nextAngle)
                {
                if (charsetEnd[0] != L' ' && charsetEnd[0] != '\'' &&
                    charsetEnd[0] != '\"' && charsetEnd[0] != '/' && charsetEnd[0] != '>')
                    { ++charsetEnd; }
                else
                    { break; }
                }
            charset.assign(start, charsetEnd-start);
            return charset;
            }
        else
            { return charset; }
        }

    //------------------------------------------------------------------
    const wchar_t* html_extract_text::stristr_not_quoted(
            const wchar_t* string, const size_t stringSize,
            const wchar_t* strSearch, const size_t strSearchSize) noexcept
        {
        if (!string || !strSearch || stringSize == 0 || strSearchSize == 0)
            { return nullptr; }

        bool is_inside_of_quotes = false;
        bool is_inside_of_single_quotes = false;
        const wchar_t* const endSentinel = string+stringSize;
        while (string && (string+strSearchSize <= endSentinel))
            {
            // compare the characters one at a time
            size_t i = 0;
            for (i = 0; i < strSearchSize; ++i)
                {
                if (string[i] == 0)
                    { return nullptr; }
                else if (string[i] == 0x22) // double quote
                    {
                    is_inside_of_quotes = !is_inside_of_quotes;
                    // whether this double quote ends a quote pair or starts a new one, turn this flag
                    // off. This means that a double quote can close a single quote.
                    is_inside_of_single_quotes = false;
                    }
                // if a single quote already started a quote pair (and this is closing it) or
                // we are not inside of a double quote then count single quotes
                else if ((!is_inside_of_quotes || is_inside_of_single_quotes) && string[0] == 0x27) // single quote
                    {
                    is_inside_of_quotes = !is_inside_of_quotes;
                    is_inside_of_single_quotes = true;
                    }
                if (std::towlower(strSearch[i]) != std::towlower(string[i]) )
                    { break; }
                }
            // if the substring loop completed then the substring was found.            
            if (i == strSearchSize)
                {
                // make sure we aren't inside of quotes--if so, we need to skip it.
                if (!is_inside_of_quotes)
                    { return string; }
                else
                    { string += strSearchSize; }
                }
            else
                { string += i+1; }
            }
        return nullptr;
        }

    //------------------------------------------------------------------
    std::pair<const wchar_t*, size_t> html_extract_text::read_attribute(const wchar_t* text,
            const wchar_t* tag, const size_t tagSize,
            const bool allowQuotedTags,
            const bool allowSpacesInValue /*= false*/)
        {
        if (!text || !tag || tagSize == 0)
            { return std::make_pair(nullptr,0); }
        assert((std::wcslen(tag) == tagSize) && "Invalid length passed to read_attribute().");
        const wchar_t* foundTag = find_tag(text, tag, tagSize, allowQuotedTags);
        const wchar_t* elementEnd = find_close_tag(text);
        if (foundTag && elementEnd &&
            foundTag < elementEnd)
            {
            foundTag += tagSize;
            // step over spaces between attribute name and its assignment operator
            while (foundTag && foundTag < elementEnd && *foundTag == L' ')
                { ++foundTag; }
            // step over assignment operator
            if (foundTag && foundTag < elementEnd && is_either(*foundTag, L':', L'='))
                { ++foundTag; }
            // step over any more spaces after assignment operator
            while (foundTag && foundTag < elementEnd && *foundTag == L' ')
                { ++foundTag; }
            // step over any opening quotes
            if (foundTag && foundTag < elementEnd && is_either(*foundTag, L'\'', L'"'))
                { ++foundTag; }
            if (foundTag >= elementEnd)
                { return std::make_pair(nullptr,0); }

            const wchar_t* end =
                (allowQuotedTags && allowSpacesInValue) ?
                    string_util::strcspn_pointer(foundTag, L"\"'>;", 4) :
                allowQuotedTags ?
                    string_util::strcspn_pointer(foundTag, L" \"'>;", 5) :
                allowSpacesInValue ?
                    string_util::strcspn_pointer(foundTag, L"\"'>", 3) :
                // not allowing spaces and the tag is not inside of quotes (like a style section)
                    string_util::strcspn_pointer(foundTag, L" \"'>", 4);
            if (end && (end <= elementEnd))
                {
                // If at the end of the element, trim off any trailing spaces or a terminating '/'.
                // Note that we don't search for '/' above because it can be inside of a valid tag value
                // (such as a file path).
                if (*end == L'>')
                    {
                    while (end-1 > foundTag)
                        {
                        if (is_either<wchar_t>(*(end-1), L'/', L' '))
                            { --end; }
                        else
                            { break; }
                        }
                    }
                if ((end-foundTag) == 0)
                    { return std::make_pair(nullptr,0); }
                return std::make_pair(foundTag, (end-foundTag));
                }
            else
                { return std::make_pair(nullptr,0); }
            }
        else
            { return std::make_pair(nullptr,0); }
        }

    //------------------------------------------------------------------
    const wchar_t* html_extract_text::find_tag(const wchar_t* text,
            const wchar_t* tag, const size_t tagSize,
            const bool allowQuotedTags)
        {
        if (!text || !tag || tagSize == 0)
            { return nullptr; }
        const wchar_t* foundTag = text;
        const wchar_t* const elementEnd = find_close_tag(text);
        if (!elementEnd)
            { return nullptr; }
        while (foundTag)
            {
            foundTag = allowQuotedTags ?
                string_util::strnistr<wchar_t>(foundTag, tag, (elementEnd-foundTag))
                : stristr_not_quoted(foundTag, (elementEnd-foundTag), tag, tagSize);
            if (!foundTag || (foundTag > elementEnd))
                { return nullptr; }
            if (foundTag == text)
                { return foundTag; }
            else if (allowQuotedTags && is_either<wchar_t>(foundTag[-1], L'\'', L'\"'))
                { return foundTag; }
            // this tag should not be count if it is really just part of a bigger tag
            // (e.g., "color" will not count if what we are really on is "bgcolor")
            else if (std::iswspace(foundTag[-1]) || (foundTag[-1] == L';'))
                { return foundTag; }
            foundTag += tagSize;
            }
        return nullptr;
        }

    //------------------------------------------------------------------
    const wchar_t* html_extract_text::operator()(const wchar_t* html_text,
                                                 const size_t text_length,
                                                 const bool include_outer_text /*= true*/,
                                                 const bool preserve_newlines /*= false*/)
        {
        static const std::wstring HTML_STYLE_END(L"</style>");
        static const std::wstring HTML_SCRIPT(L"script");
        static const std::wstring HTML_SCRIPT_END(L"</script>");
        static const std::wstring HTML_NOSCRIPT_END(L"</noscript>");
        static const std::wstring ANNOTATION_END(L"</annotation>");
        static const std::wstring ANNOTATION_XML_END(L"</annotation-xml>");
        static const std::wstring HTML_TITLE_END(L"</title>");
        static const std::wstring HTML_SUBJECT_END(L"</subject>");
        static const std::wstring HTML_COMMENT_END(L"-->");

        static const std::set<case_insensitive_wstring> newParagraphElements =
            { L"button", L"div", L"dl", L"dt", L"h1", L"h2", L"h3", L"h4",
              L"h5", L"h6", L"hr", L"input", L"ol", L"option", L"p", L"select",
              L"table", L"tr", L"ul" };

        // reset any state variables
        clear_log();
        m_is_in_preformatted_text_block_stack = preserve_newlines ? 1 : 0;
        m_superscript_stack = m_subscript_stack = 0;
        reset_meta_data();

        // verify the inputs
        if (html_text == nullptr || html_text[0] == 0 || text_length == 0)
            {
            set_filtered_text_length(0);
            return nullptr;
            }

        if (!allocate_text_buffer(text_length))
            {
            set_filtered_text_length(0);
            return nullptr;
            }

        // find the first <. If not found then just parse this as encoded HTML text
        const wchar_t* start = std::wcschr(html_text, L'<');
        if (!start)
            {
            if (include_outer_text)
                { parse_raw_text(html_text, text_length); }
            }
        // if there is text outside of the starting < section then just decode it
        else if (start > html_text && include_outer_text)
            {
            parse_raw_text(html_text, std::min<size_t>((start-html_text), text_length));
            }
        const wchar_t* end = nullptr;

        const wchar_t* const endSentinel = html_text+text_length;
        case_insensitive_wstring currentElement;
        while (start && (start < endSentinel))
            {
            const size_t remainingTextLength = (endSentinel-start);
            currentElement.assign(get_element_name(start + 1, false));
            bool isSymbolFontSection = false;
            // if it's a comment, then look for matching comment ending sequence
            if (remainingTextLength >= 4 && start[0] == L'<' &&
                start[1] == L'!' && start[2] == L'-' && start[3] == L'-')
                {
                end = std::wcsstr(start, HTML_COMMENT_END.c_str());
                // if a comment isn't properly terminated, then it's best to just assume
                // the rest of the file is a huge comment and throw it out.
                if (!end)
                    { break; }
                end += HTML_COMMENT_END.length();
                }
            // if it's a script (e.g., JavaScript), then skip it
            else if (currentElement == L"script")
                {
                end = string_util::stristr<wchar_t>(start, HTML_SCRIPT_END.c_str());
                // no closing </script>, so step over this <script> and jump to next '<'
                if (!end)
                    {
                    auto closeTag = find_close_tag(start);
                    if (!closeTag)
                        { break; }
                    end = std::wcschr(closeTag, L'<');
                    if (!end)
                        { break; }
                    }
                else
                    { end += HTML_SCRIPT_END.length(); }
                }
            // if it's a noscript (i.e., alternative text for when scripting is not available), then skip it
            else if (currentElement == L"noscript")
                {
                end = string_util::stristr<wchar_t>(start, HTML_NOSCRIPT_END.c_str());
                // no closing </noscript>, so step over this <noscript> and jump to next '<'
                if (!end)
                    {
                    auto closeTag = find_close_tag(start);
                    if (!closeTag)
                        { break; }
                    end = std::wcschr(closeTag, L'<');
                    if (!end)
                        { break; }
                    }
                else
                    { end += HTML_NOSCRIPT_END.length(); }
                }
            // if it's an annotation, then skip it
            else if (currentElement == L"annotation")
                {
                end = string_util::stristr<wchar_t>(start, ANNOTATION_END.c_str());
                // no closing </annotation>, so step over this <annotation> and jump to next '<'
                if (!end)
                    {
                    auto closeTag = find_close_tag(start);
                    if (!closeTag)
                        { break; }
                    end = std::wcschr(closeTag, L'<');
                    if (!end)
                        { break; }
                    }
                else
                    { end += ANNOTATION_END.length(); }
                }
            else if (currentElement == L"annotation-xml")
                {
                end = string_util::stristr<wchar_t>(start, ANNOTATION_XML_END.c_str());
                // no closing </annotation-xml>, so step over this <annotation-xml> and jump to next '<'
                if (!end)
                    {
                    auto closeTag = find_close_tag(start);
                    if (!closeTag)
                        { break; }
                    end = std::wcschr(closeTag, L'<');
                    if (!end)
                        { break; }
                    }
                else
                    { end += ANNOTATION_XML_END.length(); }
                }
            // if it's style command section, then skip it
            else if (currentElement == L"style")
                {
                end = string_util::stristr<wchar_t>(start, HTML_STYLE_END.c_str());
                // no closing </style>, so step over this <style> and jump to next '<'
                if (!end)
                    {
                    auto closeTag = find_close_tag(start);
                    if (!closeTag)
                        { break; }
                    end = std::wcschr(closeTag, L'<');
                    if (!end)
                        { break; }
                    }
                else
                    { end += HTML_STYLE_END.length(); }
                }
            // if it's a meta element
            else if (currentElement == L"meta")
                {
                const std::wstring metaName = lily_of_the_valley::html_extract_text::read_attribute_as_string(start,
                        L"name", 4, false);
                auto closeTag = find_close_tag(start);
                if (!closeTag)
                    { break; }
                if (string_util::stricmp(metaName.c_str(), L"author") == 0)
                    {
                    m_author = lily_of_the_valley::html_extract_text::read_attribute_as_string(start,
                        L"content", 7, false, true);
                    html_extract_text valueParser;
                    auto author = valueParser(m_author.c_str(), m_author.length(), true, false);
                    if (author)
                        {
                        m_author.assign(author);
                        string_util::trim(m_author);
                        string_util::remove_extra_spaces(m_author);
                        }
                    }
                else if (string_util::stricmp(metaName.c_str(), L"description") == 0)
                    {
                    m_description = lily_of_the_valley::html_extract_text::read_attribute_as_string(start,
                        L"content", 7, false, true);
                    html_extract_text valueParser;
                    auto description = valueParser(m_description.c_str(), m_description.length(), true, false);
                    if (description)
                        {
                        m_description.assign(description);
                        string_util::trim(m_description);
                        string_util::remove_extra_spaces(m_description);
                        }
                    }
                else if (string_util::stricmp(metaName.c_str(), L"keywords") == 0)
                    {
                    m_keywords = lily_of_the_valley::html_extract_text::read_attribute_as_string(start,
                        L"content", 7, false, true);
                    html_extract_text valueParser;
                    auto keywords = valueParser(m_keywords.c_str(), m_keywords.length(), true, false);
                    if (keywords)
                        {
                        m_keywords.assign(keywords);
                        string_util::trim(m_keywords);
                        string_util::remove_extra_spaces(m_keywords);
                        }
                    }
                // move to next element
                end = std::wcschr(closeTag, L'<');
                if (!end)
                    { break; }
                }
            // if it's a title, then look for matching title ending sequence
            else if (currentElement == L"title")
                {
                auto titleStart = find_close_tag(start);
                if (!titleStart)
                    { break; }
                ++titleStart; // step over '>'
                end = string_util::stristr<wchar_t>(start, HTML_TITLE_END.c_str());
                // <title> not closed with </title>, so step over <title> and move to next '<'
                if (!end)
                    {
                    end = std::wcschr(titleStart, L'<');
                    if (!end)
                        { break; }
                    }
                else
                    {
                    // convert any embedded entities in the title
                    html_extract_text titleParser;
                    auto title = titleParser(titleStart, end-titleStart, true, false);
                    if (title)
                        {
                        m_title.assign(title);
                        string_util::trim(m_title);
                        string_util::remove_extra_spaces(m_title);
                        }
                    end += HTML_TITLE_END.length();
                    }
                }
            // if it's a subject (not standard HTML, but Library of Congress uses this)
            else if (currentElement == L"subject")
                {
                auto subjectStart = find_close_tag(start);
                if (!subjectStart)
                    { break; }
                ++subjectStart; // step over '>'
                end = string_util::stristr<wchar_t>(start, HTML_SUBJECT_END.c_str());
                // <subject> not closed with </subject>, so step over <subject> and move to next '<'
                if (!end)
                    {
                    end = std::wcschr(subjectStart, L'<');
                    if (!end)
                        { break; }
                    }
                else
                    {
                    // convert any embedded entities in the subject
                    html_extract_text subjectParser;
                    auto subject = subjectParser(subjectStart, end-subjectStart, true, false);
                    if (subject)
                        {
                        m_subject.assign(subject);
                        string_util::trim(m_subject);
                        string_util::remove_extra_spaces(m_subject);
                        }
                    end += HTML_SUBJECT_END.length();
                    }
                }
            // stray < (i.e., < wasn't encoded) should be treated as such, instead of a tag
            else if ((remainingTextLength >= 2 && start[0] == L'<' && std::iswspace(start[1])) ||
                (remainingTextLength >= 7 && start[0] == L'<' &&
                start[1] == L'&' && is_either<wchar_t>(start[2], L'n', L'N') &&
                is_either<wchar_t>(start[3], L'b', L'B') &&
                is_either<wchar_t>(start[4], L's', L'S') &&
                is_either<wchar_t>(start[5], L'p', L'P') &&
                start[6] == L';'))
                {
                end = std::wcschr(start+1, L'<');
                if (!end)
                    {
                    parse_raw_text(start, endSentinel-start);
                    break;
                    }
                /* copy over the text from the unterminated < to the currently found
                   < (that we will start from in the next loop*/
                parse_raw_text(start, end-start);
                // set the starting point to the next < that we already found
                start = end;
                continue;
                }
            // read in ![CDATA[ date blocks as they appear (no HTML conversation happens here)
            else if (currentElement.compare(0, 8, L"![CDATA[", 8) == 0)
                {
                start += 9;
                end = std::wcsstr(start, L"]]>");
                if (!end || end > endSentinel)
                    {
                    ++m_is_in_preformatted_text_block_stack; // preserve newline formatting
                    parse_raw_text(start, endSentinel-start);
                    --m_is_in_preformatted_text_block_stack;
                    break;
                    }
                add_characters(start, end-start);
                start = end;
                end += 2;
                continue;
                }
            else
                {
                // Symbol font section (we will need to do some special formatting later).
                // First, special logic for "font" element...
                if (currentElement == L"font")
                    {
                    if (string_util::strnicmp(read_attribute(start+1, L"face", 4, false, true).first, L"Symbol", 6) == 0 ||
                        string_util::strnicmp(read_attribute(start+1, L"font-family", 11, true, true).first, L"Symbol", 6) == 0)
                        { isSymbolFontSection = true; }
                    }
                // ...then any other element
                else
                    {
                    if (string_util::strnicmp(read_attribute(start+1, L"font-family", 11, true, true).first, L"Symbol", 6) == 0)
                        { isSymbolFontSection = true; }
                    }
                // see if this is a preformatted section, where CRLFs should be preserved
                if (currentElement == L"pre")
                    { ++m_is_in_preformatted_text_block_stack; }
                else if (currentElement == L"sup")
                    { ++m_superscript_stack; }
                else if (currentElement == L"sub")
                    { ++m_subscript_stack; }
                // see if this should be treated as a new paragraph because it is a break,
                // paragraph, list item, or table row
                else if (newParagraphElements.find(currentElement) != newParagraphElements.cend())
                    {
                    add_character(L'\n');
                    add_character(L'\n');
                    // insert a page break before this section of text if requested.
                    const std::wstring pageBreakValue = read_attribute_as_string(start+1, L"page-break-before", 17, true, false);
                    if (pageBreakValue.length() &&
                        (string_util::strnicmp(pageBreakValue.c_str(), L"always", 6) == 0||
                         string_util::strnicmp(pageBreakValue.c_str(), L"auto", 4) == 0 ||
                         string_util::strnicmp(pageBreakValue.c_str(), L"left", 4) == 0 ||
                         string_util::strnicmp(pageBreakValue.c_str(), L"right", 5) == 0))
                        { add_character(L'\f'); }
                    }
                else if (currentElement == L"br")
                    { add_character(L'\n'); }
                // or end of a section that is like a paragraph
                else if (remainingTextLength >= 3 && start[0] == L'<' && start[1] == L'/')
                    {
                    if (newParagraphElements.find(currentElement.substr(1)) != newParagraphElements.cend() &&
                        // no need for extra lines for these, the terminating
                        // </table>, </dl>, or </select> will handle that
                        currentElement != L"/tr" && currentElement != L"/dt" && currentElement != L"/option")
                        {
                        add_character(L'\n');
                        add_character(L'\n');
                        }
                    }
                else if (currentElement == L"li")
                    {
                    add_character(L'\n');
                    add_character(L'\t');
                    }
                // or tab over table cell
                else if (currentElement == L"td")
                    { add_character(L'\t'); }
                // format a definition to have a colon between the term and its upcoming definition
                else if (currentElement == L"dd")
                    {
                    add_character(L':');
                    add_character(L'\t');
                    }
                // hyperlinks
                else if (currentElement == L"a")
                    {
                    // often e-mail and telephone links are missing the space between them and the proceeding word,
                    // so force one in front of it just to be safe.
                        {
                        const auto attrib = read_attribute_as_string(start+1, L"href", 4, false, false);
                        if (string_util::strnicmp(attrib.c_str(), L"mailto:", 7) == 0 ||
                            string_util::strnicmp(attrib.c_str(), L"tel:", 4) == 0)
                            { add_character(L' '); }
                        }
                    // links that would usually be its own line
                        {
                        const auto attrib = read_attribute_as_string(start+1, L"class", 5, false, false);
                        if (attrib.find(L"FooterLink") != std::wstring::npos)
                            {
                            add_character(L'\n');
                            add_character(L'\n');
                            }
                        }
                    }
                else if (currentElement == L"span")
                    {
                    // span data-types that could cause a new paragraph or tab
                        {
                        const auto attrib = read_attribute_as_string(start+1, L"data-type", 9, false, false);
                        // non-standard way of saying <br />
                        if (attrib == L"newline")
                            { add_character(L'\n'); }
                        else if (attrib == L"footnote-ref-content")
                            { add_character(L'\t'); }
                        }
                    // span class that could cause a new paragraph (or be hidden)
                        {
                        const auto attrib = read_attribute_as_string(start+1, L"class", 5, false, false);
                        if (attrib.length())
                            {
                            if (attrib.find(L"BookBanner") != std::wstring::npos ||
                                attrib == L"os-caption")
                                {
                                add_character(L'\n');
                                add_character(L'\n');
                                }
                            else if (attrib == L"os-term-section")
                                { add_character(L'\t'); }
                            else if (attrib.find(L"hidden") != std::wstring::npos)
                                {
                                auto spanEnd = find_closing_element(start, endSentinel, L"span", 4);
                                if (spanEnd)
                                    { start = spanEnd; }
                                }
                            }
                        }
                    }
                end = find_close_tag(start+1);
                if (!end)
                    {
                    // no close tag? read to the next open tag then and read this section in below
                    if ((end = std::wcschr(start+1, L'<')) == nullptr)
                        { break; }
                    }
                /* if the < tag that we started from is not terminated then feed that in as
                   text instead of treating it like a valid HTML tag.  Not common, but it happens.*/
                if (end[0] == L'<')
                    {
                    /* copy over the text from the unterminated < to the currently found
                       < (that we will start from in the next loop*/
                    parse_raw_text(start, end-start);
                    // set the starting point to the next < that we already found
                    start = end;
                    continue;
                    }
                // more normal behavior, where tag is properly terminated
                else
                    { ++end; }
                }
            // find the next starting tag
            start = std::wcschr(end, L'<');
            if (!start || start >= endSentinel)
                { break; }
            // cache length before reparsing
            const auto previousLength = get_filtered_text_length();
            // copy over the text between the tags
            parse_raw_text(end, start-end);
            /* Old HTML (or HTML generated from embarrassingly hackneyed WYSIWYG editors) uses the "Symbol" font
               to show various math/Greek symbols (instead of proper entities).
               So if the current block of text is using the font "Symbol," then we will convert
               it to the expected symbol.*/
            if (isSymbolFontSection)
                {
                const std::wstring copiedOverText =
                    convert_symbol_font_section(std::wstring_view(get_filtered_text() + previousLength,
                                                                  // cppcheck-suppress duplicateExpression
                                                                  get_filtered_text_length() - previousLength) );
                set_filtered_text_length(previousLength);
                add_characters(copiedOverText.c_str(), copiedOverText.length());
                if (copiedOverText.length())
                    { log_message(L"Symbol font used for the following: \"" + copiedOverText + L"\""); }
                }
            // after parsing this section, see if this is the end of a preformatted area
            if (string_util::strnicmp<wchar_t>(start, L"</pre>", 6) == 0)
                {
                if (m_is_in_preformatted_text_block_stack > 0)
                    { --m_is_in_preformatted_text_block_stack; }
                }
            // same for superscripts
            else if (string_util::strnicmp<wchar_t>(start, L"</sup>", 6) == 0)
                {
                if (m_superscript_stack > 0)
                    { --m_superscript_stack; }
                }
            // and subscripts
            else if (string_util::strnicmp<wchar_t>(start, L"</sub>", 6) == 0)
                {
                if (m_subscript_stack > 0)
                    { --m_subscript_stack; }
                }
            }

        // get any text lingering after the last >
        if (end && end < endSentinel && include_outer_text)
            {
            parse_raw_text(end, endSentinel-end);
            }

        return get_filtered_text();
        }

    //------------------------------------------------------------------
    bool html_extract_text::compare_element(const wchar_t* text, const wchar_t* element,
                                            const size_t element_size,
                                            const bool accept_self_terminating_elements /*= false*/)
        {
        if (!text || !element || element_size == 0)
            { return false; }
        assert((std::wcslen(element) == element_size) && "Invalid length passed to compare_element().");
        // first see if the element matches the text (e.g., "br" or "br/" [if accepting self terminating element])
        if (string_util::strnicmp(text, element, element_size) == 0)
            {
            /* now we need to make sure that there isn't more to the element in the text.
               In other words, verify that it is either terminated by a '>' or proceeded with
               attributes; otherwise, the element in the text is not the same as the element
               that we are comparing against.*/
            text += element_size;
            // if element is missing '>' and has nothing after it then it's invalid.
            if (*text == 0)
                { return false; }
            // if immediately closed then it's valid.
            else if (*text == L'>')
                { return true; }
            // if we are allowing terminated elements ("/>") then it's a match if we
            // are on a '/'. Otherwise, if not a space after it then fail. If it is a space,
            // then we need to scan beyond that to make sure it isn't self terminated after the space.
            else if (accept_self_terminating_elements)
                {
                return (*text == L'/' ||
                        std::iswspace(*text));
                }
            // if we aren't allowing "/>" and we are on a space, then just make sure
            // it isn't self terminated.
            else if (std::iswspace(*text))
                {
                const wchar_t* closeTag = find_close_tag(text);
                if (!closeTag)
                    { return false; }
                --closeTag;
                while (closeTag > text &&
                    std::iswspace(*closeTag))
                    { --closeTag; }
                return (*closeTag != L'/');
                }
            else
                { return false; }
            }
        else
            { return false; }
        }

    //------------------------------------------------------------------
    bool html_extract_text::compare_element_case_sensitive(const wchar_t* text, const wchar_t* element,
                                                           const size_t element_size,
                                                           const bool accept_self_terminating_elements /*= false*/)
        {
        if (!text || !element || element_size == 0)
            { return false; }
        assert((std::wcslen(element) == element_size) && "Invalid length passed to compare_element().");
        // first see if the element matches the text (e.g., "br" or "br/" [if accepting self terminating element])
        if (std::wcsncmp(text, element, element_size) == 0)
            {
            /* now we need to make sure that there isn't more to the element in the text.
               In other words, verify that it is either terminated by a '>' or proceeded with
               attributes; otherwise, the element in the text is not the same as the element
               that we are comparing against.*/
            text += element_size;
            // if element is missing '>' and has nothing after it then it's invalid.
            if (*text == 0)
                { return false; }
            // if immediately closed then it's valid.
            else if (*text == L'>')
                { return true; }
            // if we are allowing terminated elements ("/>") then it's a match if we
            // are on a '/'. Otherwise, if not a space after it then fail. If it is a space,
            // then we need to scan beyond that to make sure it isn't self terminated after the space.
            else if (accept_self_terminating_elements)
                {
                return (*text == L'/' ||
                        std::iswspace(*text));
                }
            // if we aren't allowing "/>" and we are on a space, then just make sure
            // it isn't self terminated.
            else if (std::iswspace(*text))
                {
                const wchar_t* closeTag = find_close_tag(text);
                if (!closeTag)
                    { return false; }
                --closeTag;
                while (closeTag > text &&
                    std::iswspace(*closeTag))
                    { --closeTag; }
                return (*closeTag != L'/');
                }
            else
                { return false; }
            }
        else
            { return false; }
        }

    //------------------------------------------------------------------
    std::wstring html_extract_text::get_body(const std::wstring_view& text)
        {
        size_t bodyStart = text.find(L"<body");
        if (bodyStart != std::wstring::npos)
            {
            bodyStart = text.find(L'>', bodyStart);
            if (bodyStart == std::wstring::npos)
                { return std::wstring{ text }; } // ill-formed file
            ++bodyStart;
            const size_t bodyEnd = text.find(L"</body>", bodyStart);
            if (bodyEnd != std::wstring::npos)
                { return std::wstring(text.substr(bodyStart, bodyEnd-bodyStart)); }
            }
        // no body tags found, so assume the whole thing is the body
        return std::wstring{ text };
        }

    //------------------------------------------------------------------
    std::wstring html_extract_text::get_style_section(const std::wstring_view& text)
        {
        size_t styleStart = text.find(L"<style");
        if (styleStart != std::wstring_view::npos)
            {
            styleStart = text.find(L'>',styleStart);
            if (styleStart == std::wstring_view::npos)
                { return std::wstring{}; } // ill-formed file
            const size_t styleEnd = text.find(L"</style>", styleStart);
            if (styleEnd != std::wstring_view::npos)
                {
                ++styleStart;
                std::wstring styleSection(text.substr(styleStart, styleEnd-(styleStart)));
                string_util::trim(styleSection);
                if (styleSection.length() > 4 &&
                    styleSection.compare(0, 4, L"<!--") == 0)
                    { styleSection.erase(0, 4); }
                if (styleSection.length() > 3 &&
                    styleSection.compare(styleSection.length()-3, 3, L"-->") == 0)
                    { styleSection.erase(styleSection.length()-3); }
                string_util::trim(styleSection);
                return styleSection;
                }
            }
        // no style tags found, so not much to work with here
        return std::wstring{};
        }

    //------------------------------------------------------------------
    case_insensitive_wstring_view html_extract_text::get_element_name(
        const wchar_t* text, const bool accept_self_terminating_elements /*= true*/)
        {
        if (text == nullptr)
            { return case_insensitive_wstring_view{}; }
        const wchar_t* start = text;
        for (;;)
            {
            if (text[0] == 0 ||
                std::iswspace(text[0]) ||
                text[0] == L'>')
                { break; }
            else if (accept_self_terminating_elements &&
                text[0] == L'/' &&
                text[1] == L'>')
                { break; }
            ++text;
            }
        return case_insensitive_wstring_view(start, text-start);
        }

    //------------------------------------------------------------------
    const wchar_t* html_extract_text::find_close_tag(const wchar_t* text) noexcept
        {
        if (text == nullptr)
            { return nullptr; }
        // if we are at the beginning of an open statement, skip the opening < so that we can correctly
        // look for the next opening <
        else if (text[0] == L'<')
            { ++text; }

        bool is_inside_of_quotes = false;
        bool is_inside_of_single_quotes = false;
        long openTagCount{ 0 };
        while (text)
            {
            if (text[0] == 0)
                { return nullptr; }
            else if (text[0] == 0x22) // double quote
                {
                is_inside_of_quotes = !is_inside_of_quotes;
                // whether this double quote ends a quote pair or starts a new one, turn this flag
                // off. This means that a double quote can close a single quote.
                is_inside_of_single_quotes = false;
                }
            // if a single quote already started a quote pair (and this is closing it) or
            // we are not inside of a double quote then count single quotes
            else if ((!is_inside_of_quotes || is_inside_of_single_quotes) && text[0] == 0x27) //single quote
                {
                is_inside_of_quotes = !is_inside_of_quotes;
                is_inside_of_single_quotes = true;
                }
            else if (!is_inside_of_quotes && text[0] == L'<')
                { ++openTagCount; }
            else if (!is_inside_of_quotes && text[0] == L'>')
                {
                if (openTagCount == 0)
                    { return text; }
                else
                    { --openTagCount; }
                }
            ++text;
            }
        return nullptr;
        }

    //------------------------------------------------------------------
    const wchar_t* html_extract_text::find_element(const wchar_t* sectionStart,
                                           const wchar_t* sectionEnd,
                                           const wchar_t* elementTag,
                                           const size_t elementTagLength,
                                           const bool accept_self_terminating_elements /*= true*/)
        {
        if (sectionStart == nullptr || sectionEnd == nullptr ||
            elementTag == nullptr || elementTagLength == 0)
            { return nullptr; }
        assert((std::wcslen(elementTag) == elementTagLength) &&
               "Invalid length passed to find_element().");
        while (sectionStart && sectionStart+elementTagLength < sectionEnd)
            {
            sectionStart = std::wcschr(sectionStart, L'<');
            if (sectionStart == nullptr || sectionStart+elementTagLength > sectionEnd)
                { return nullptr; }
            else if (compare_element(sectionStart+1, elementTag, elementTagLength,
                                     accept_self_terminating_elements))
                { return sectionStart; }
            else
                { sectionStart += 1 /* skip the '<' and search for the next one*/; }
            }
        return nullptr;
        }

    //------------------------------------------------------------------
    const wchar_t* html_extract_text::find_closing_element(const wchar_t* sectionStart,
                                           const wchar_t* sectionEnd,
                                           const wchar_t* elementTag,
                                           const size_t elementTagLength)
        {
        if (sectionStart == nullptr || sectionEnd == nullptr ||
            elementTag == nullptr || elementTagLength == 0)
            { return nullptr; }
        assert((std::wcslen(elementTag) == elementTagLength) &&
               "Invalid length passed to find_closing_element().");
        const wchar_t* start = std::wcschr(sectionStart, L'<');
        if (start == nullptr || start+elementTagLength > sectionEnd)
            { return nullptr; }
        ++start; // skip '<'
        // if we are on an opening element by the same name, then skip it so that we won't
        // count it again in the stack logic below
        if (compare_element(start, elementTag, elementTagLength, true))
            { sectionStart = start+elementTagLength; }
        // else if we are on the closing element already then just return that.
        else if (start[0] == '/' && compare_element(start+1, elementTag, elementTagLength, true))
            { return --start; }

        // Do a search for the matching close tag. That means
        // that it will skip any inner elements that are the same element and go
        // to the correct closing one.
        long stackSize = 1;

        start = std::wcschr(sectionStart, L'<');
        while (start && start+elementTagLength+1 < sectionEnd)
            {
            // if a closing element if found, then decrease the stack
            if (start[1] == L'/' && compare_element(start+2, elementTag, elementTagLength, true))
                { --stackSize; }
            // if a new opening element by the same name, then add that to the stack so that its
            // respective closing element will be skipped.
            else if (compare_element(start+1, elementTag, elementTagLength, true))
                { ++stackSize; }
            if (stackSize == 0)
                { return start; }
            start = std::wcschr(start+1, L'<');
            }
        return nullptr;
        }

    const html_utilities::symbol_font_table html_extract_text::SYMBOL_FONT_TABLE;
    const html_utilities::html_entity_table html_extract_text::HTML_TABLE_LOOKUP;
    }

namespace html_utilities
    {
    //------------------------------------------------------------------
    const wchar_t* html_strip_hyperlinks::operator()(const wchar_t* html_text,
                                  const size_t text_length)
        {
        if (html_text == nullptr || html_text[0] == 0 || text_length == 0)
            { return nullptr; }
        assert(text_length <= std::wcslen(html_text) );

        if (!allocate_text_buffer(text_length))
            { return nullptr; }

        const wchar_t* const endSentinel = html_text+text_length;
        const wchar_t* currentPos = html_text, *lastEnd = html_text;
        while (currentPos && (currentPos < endSentinel))
            {
            currentPos = lily_of_the_valley::html_extract_text::find_element(currentPos, endSentinel, L"a", 1, true);
            // no more anchors, so just copy over the rest of the text and quit.
            if (!currentPos || currentPos >= endSentinel)
                {
                add_characters(lastEnd, endSentinel-lastEnd);
                break;
                }
            // if this is actually a bookmark, then we need to start over (looking for the next <a>).
            if (lily_of_the_valley::html_extract_text::find_tag(currentPos, L"name", 4, false))
                {
                currentPos += 2;
                continue;
                }
            // next <a> found, so copy over all of the text before it,
            // then move over to the end of this element.
            add_characters(lastEnd, currentPos-lastEnd);
            currentPos = lily_of_the_valley::html_extract_text::find_close_tag(currentPos);
            if (!currentPos || currentPos >= endSentinel)
                { break; }
            lastEnd = currentPos+1;
            // ...Now, find the matching </a> and copy over the text between that and the previous <a>.
            // Note that nested <a> would be very incorrect HTML, so we will safely assume that there aren't any.
            currentPos = lily_of_the_valley::html_extract_text::find_closing_element(currentPos, endSentinel, L"a", 1);
            if (!currentPos || currentPos >= endSentinel)
                { break; }
            add_characters(lastEnd, currentPos-lastEnd);
            // ...finally, find the close of this </a>, move to that, and start over again looking for the next <a>
            currentPos = lily_of_the_valley::html_extract_text::find_close_tag(currentPos);
            if (!currentPos || currentPos >= endSentinel)
                { break; }
            lastEnd = currentPos+1;
            }
        return get_filtered_text();
        }

    //------------------------------------------------------------------
    symbol_font_table::symbol_font_table()
        {
        // Greek alphabet
        // cppcheck-suppress useInitializationList
        m_symbol_table =
            {
            std::make_pair(static_cast<wchar_t>(L'A'), static_cast<wchar_t>(913)), // uppercase Alpha
            std::make_pair(static_cast<wchar_t>(L'B'), static_cast<wchar_t>(914)), // uppercase Beta
            std::make_pair(static_cast<wchar_t>(L'G'), static_cast<wchar_t>(915)), // uppercase Gamma
            std::make_pair(static_cast<wchar_t>(L'D'), static_cast<wchar_t>(916)), // uppercase Delta
            std::make_pair(static_cast<wchar_t>(L'E'), static_cast<wchar_t>(917)), // uppercase Epsilon
            std::make_pair(static_cast<wchar_t>(L'Z'), static_cast<wchar_t>(918)), // uppercase Zeta
            std::make_pair(static_cast<wchar_t>(L'H'), static_cast<wchar_t>(919)), // uppercase Eta
            std::make_pair(static_cast<wchar_t>(L'Q'), static_cast<wchar_t>(920)), // uppercase Theta
            std::make_pair(static_cast<wchar_t>(L'I'), static_cast<wchar_t>(921)), // uppercase Iota
            std::make_pair(static_cast<wchar_t>(L'K'), static_cast<wchar_t>(922)), // uppercase Kappa
            std::make_pair(static_cast<wchar_t>(L'L'), static_cast<wchar_t>(923)), // uppercase Lambda
            std::make_pair(static_cast<wchar_t>(L'M'), static_cast<wchar_t>(924)), // uppercase Mu
            std::make_pair(static_cast<wchar_t>(L'N'), static_cast<wchar_t>(925)), // uppercase Nu
            std::make_pair(static_cast<wchar_t>(L'X'), static_cast<wchar_t>(926)), // uppercase Xi
            std::make_pair(static_cast<wchar_t>(L'O'), static_cast<wchar_t>(927)), // uppercase Omicron
            std::make_pair(static_cast<wchar_t>(L'P'), static_cast<wchar_t>(928)), // uppercase Pi
            std::make_pair(static_cast<wchar_t>(L'R'), static_cast<wchar_t>(929)), // uppercase Rho
            std::make_pair(static_cast<wchar_t>(L'S'), static_cast<wchar_t>(931)), // uppercase Sigma
            std::make_pair(static_cast<wchar_t>(L'T'), static_cast<wchar_t>(932)), // uppercase Tau
            std::make_pair(static_cast<wchar_t>(L'U'), static_cast<wchar_t>(933)), // uppercase Upsilon
            std::make_pair(static_cast<wchar_t>(L'F'), static_cast<wchar_t>(934)), // uppercase Phi
            std::make_pair(static_cast<wchar_t>(L'C'), static_cast<wchar_t>(935)), // uppercase Chi
            std::make_pair(static_cast<wchar_t>(L'Y'), static_cast<wchar_t>(936)), // uppercase Psi
            std::make_pair(static_cast<wchar_t>(L'W'), static_cast<wchar_t>(937)), // uppercase Omega
            std::make_pair(static_cast<wchar_t>(L'a'), static_cast<wchar_t>(945)), // lowercase alpha
            std::make_pair(static_cast<wchar_t>(L'b'), static_cast<wchar_t>(946)), // lowercase beta
            std::make_pair(static_cast<wchar_t>(L'g'), static_cast<wchar_t>(947)), // lowercase gamma
            std::make_pair(static_cast<wchar_t>(L'd'), static_cast<wchar_t>(948)), // lowercase delta
            std::make_pair(static_cast<wchar_t>(L'e'), static_cast<wchar_t>(949)), // lowercase epsilon
            std::make_pair(static_cast<wchar_t>(L'z'), static_cast<wchar_t>(950)), // lowercase zeta
            std::make_pair(static_cast<wchar_t>(L'h'), static_cast<wchar_t>(951)), // lowercase eta
            std::make_pair(static_cast<wchar_t>(L'q'), static_cast<wchar_t>(952)), // lowercase theta
            std::make_pair(static_cast<wchar_t>(L'i'), static_cast<wchar_t>(953)), // lowercase iota
            std::make_pair(static_cast<wchar_t>(L'k'), static_cast<wchar_t>(954)), // lowercase kappa
            std::make_pair(static_cast<wchar_t>(L'l'), static_cast<wchar_t>(955)), // lowercase lambda
            std::make_pair(static_cast<wchar_t>(L'm'), static_cast<wchar_t>(956)), // lowercase mu
            std::make_pair(static_cast<wchar_t>(L'n'), static_cast<wchar_t>(957)), // lowercase nu
            std::make_pair(static_cast<wchar_t>(L'x'), static_cast<wchar_t>(958)), // lowercase xi
            std::make_pair(static_cast<wchar_t>(L'o'), static_cast<wchar_t>(959)), // lowercase omicron
            std::make_pair(static_cast<wchar_t>(L'p'), static_cast<wchar_t>(960)), // lowercase pi
            std::make_pair(static_cast<wchar_t>(L'r'), static_cast<wchar_t>(961)), // lowercase rho
            std::make_pair(static_cast<wchar_t>(L'V'), static_cast<wchar_t>(962)), // uppercase sigmaf
            std::make_pair(static_cast<wchar_t>(L's'), static_cast<wchar_t>(963)), // lowercase sigma
            std::make_pair(static_cast<wchar_t>(L't'), static_cast<wchar_t>(964)), // lowercase tau
            std::make_pair(static_cast<wchar_t>(L'u'), static_cast<wchar_t>(965)), // lowercase upsilon
            std::make_pair(static_cast<wchar_t>(L'f'), static_cast<wchar_t>(966)), // lowercase phi
            std::make_pair(static_cast<wchar_t>(L'c'), static_cast<wchar_t>(967)), // lowercase chi
            std::make_pair(static_cast<wchar_t>(L'y'), static_cast<wchar_t>(968)), // lowercase psi
            std::make_pair(static_cast<wchar_t>(L'w'), static_cast<wchar_t>(969)), // lowercase omega
            std::make_pair(static_cast<wchar_t>(L'J'), static_cast<wchar_t>(977)), // uppercase thetasym
            std::make_pair(static_cast<wchar_t>(161), static_cast<wchar_t>(978)),  // lowercase Upsilon1
            std::make_pair(static_cast<wchar_t>(L'j'), static_cast<wchar_t>(981)), // lowercase phi1
            std::make_pair(static_cast<wchar_t>(L'v'), static_cast<wchar_t>(982)), // lowercase omega1
            // arrows
            std::make_pair(static_cast<wchar_t>(171), static_cast<wchar_t>(8596)),
            std::make_pair(static_cast<wchar_t>(172), static_cast<wchar_t>(8592)),
            std::make_pair(static_cast<wchar_t>(173), static_cast<wchar_t>(8593)),
            std::make_pair(static_cast<wchar_t>(174), static_cast<wchar_t>(8594)),
            std::make_pair(static_cast<wchar_t>(175), static_cast<wchar_t>(8595)),
            std::make_pair(static_cast<wchar_t>(191), static_cast<wchar_t>(8629)),
            std::make_pair(static_cast<wchar_t>(219), static_cast<wchar_t>(8660)),
            std::make_pair(static_cast<wchar_t>(220), static_cast<wchar_t>(8656)),
            std::make_pair(static_cast<wchar_t>(221), static_cast<wchar_t>(8657)),
            std::make_pair(static_cast<wchar_t>(222), static_cast<wchar_t>(8658)),
            std::make_pair(static_cast<wchar_t>(223), static_cast<wchar_t>(8659)),
            // math
            std::make_pair(static_cast<wchar_t>(34), static_cast<wchar_t>(8704)),
            std::make_pair(static_cast<wchar_t>(36), static_cast<wchar_t>(8707)),
            std::make_pair(static_cast<wchar_t>(39), static_cast<wchar_t>(8717)),
            std::make_pair(static_cast<wchar_t>(42), static_cast<wchar_t>(8727)),
            std::make_pair(static_cast<wchar_t>(45), static_cast<wchar_t>(8722)),
            std::make_pair(static_cast<wchar_t>(64), static_cast<wchar_t>(8773)),
            std::make_pair(static_cast<wchar_t>(92), static_cast<wchar_t>(8756)),
            std::make_pair(static_cast<wchar_t>(94), static_cast<wchar_t>(8869)),
            std::make_pair(static_cast<wchar_t>(126), static_cast<wchar_t>(8764)),
            std::make_pair(static_cast<wchar_t>(163), static_cast<wchar_t>(8804)),
            std::make_pair(static_cast<wchar_t>(165), static_cast<wchar_t>(8734)),
            std::make_pair(static_cast<wchar_t>(179), static_cast<wchar_t>(8805)),
            std::make_pair(static_cast<wchar_t>(181), static_cast<wchar_t>(8733)),
            std::make_pair(static_cast<wchar_t>(182), static_cast<wchar_t>(8706)),
            std::make_pair(static_cast<wchar_t>(183), static_cast<wchar_t>(8729)),
            std::make_pair(static_cast<wchar_t>(185), static_cast<wchar_t>(8800)),
            std::make_pair(static_cast<wchar_t>(186), static_cast<wchar_t>(8801)),
            std::make_pair(static_cast<wchar_t>(187), static_cast<wchar_t>(8776)),
            std::make_pair(static_cast<wchar_t>(196), static_cast<wchar_t>(8855)),
            std::make_pair(static_cast<wchar_t>(197), static_cast<wchar_t>(8853)),
            std::make_pair(static_cast<wchar_t>(198), static_cast<wchar_t>(8709)),
            std::make_pair(static_cast<wchar_t>(199), static_cast<wchar_t>(8745)),
            std::make_pair(static_cast<wchar_t>(200), static_cast<wchar_t>(8746)),
            std::make_pair(static_cast<wchar_t>(201), static_cast<wchar_t>(8835)),
            std::make_pair(static_cast<wchar_t>(202), static_cast<wchar_t>(8839)),
            std::make_pair(static_cast<wchar_t>(203), static_cast<wchar_t>(8836)),
            std::make_pair(static_cast<wchar_t>(204), static_cast<wchar_t>(8834)),
            std::make_pair(static_cast<wchar_t>(205), static_cast<wchar_t>(8838)),
            std::make_pair(static_cast<wchar_t>(206), static_cast<wchar_t>(8712)),
            std::make_pair(static_cast<wchar_t>(207), static_cast<wchar_t>(8713)),
            std::make_pair(static_cast<wchar_t>(208), static_cast<wchar_t>(8736)),
            std::make_pair(static_cast<wchar_t>(209), static_cast<wchar_t>(8711)),
            std::make_pair(static_cast<wchar_t>(213), static_cast<wchar_t>(8719)),
            std::make_pair(static_cast<wchar_t>(214), static_cast<wchar_t>(8730)),
            std::make_pair(static_cast<wchar_t>(215), static_cast<wchar_t>(8901)),
            std::make_pair(static_cast<wchar_t>(217), static_cast<wchar_t>(8743)),
            std::make_pair(static_cast<wchar_t>(218), static_cast<wchar_t>(8744)),
            std::make_pair(static_cast<wchar_t>(229), static_cast<wchar_t>(8721)),
            std::make_pair(static_cast<wchar_t>(242), static_cast<wchar_t>(8747)),
            std::make_pair(static_cast<wchar_t>(224), static_cast<wchar_t>(9674)),
            std::make_pair(static_cast<wchar_t>(189), static_cast<wchar_t>(9168)),
            std::make_pair(static_cast<wchar_t>(190), static_cast<wchar_t>(9135)),
            std::make_pair(static_cast<wchar_t>(225), static_cast<wchar_t>(9001)),
            std::make_pair(static_cast<wchar_t>(230), static_cast<wchar_t>(9115)),
            std::make_pair(static_cast<wchar_t>(231), static_cast<wchar_t>(9116)),
            std::make_pair(static_cast<wchar_t>(232), static_cast<wchar_t>(9117)),
            std::make_pair(static_cast<wchar_t>(233), static_cast<wchar_t>(9121)),
            std::make_pair(static_cast<wchar_t>(234), static_cast<wchar_t>(9122)),
            std::make_pair(static_cast<wchar_t>(235), static_cast<wchar_t>(9123)),
            std::make_pair(static_cast<wchar_t>(236), static_cast<wchar_t>(9127)),
            std::make_pair(static_cast<wchar_t>(237), static_cast<wchar_t>(9128)),
            std::make_pair(static_cast<wchar_t>(238), static_cast<wchar_t>(9129)),
            std::make_pair(static_cast<wchar_t>(239), static_cast<wchar_t>(9130)),
            std::make_pair(static_cast<wchar_t>(241), static_cast<wchar_t>(9002)),
            std::make_pair(static_cast<wchar_t>(243), static_cast<wchar_t>(8992)),
            std::make_pair(static_cast<wchar_t>(244), static_cast<wchar_t>(9134)),
            std::make_pair(static_cast<wchar_t>(245), static_cast<wchar_t>(8993)),
            std::make_pair(static_cast<wchar_t>(246), static_cast<wchar_t>(9118)),
            std::make_pair(static_cast<wchar_t>(247), static_cast<wchar_t>(9119)),
            std::make_pair(static_cast<wchar_t>(248), static_cast<wchar_t>(9120)),
            std::make_pair(static_cast<wchar_t>(249), static_cast<wchar_t>(9124)),
            std::make_pair(static_cast<wchar_t>(250), static_cast<wchar_t>(9125)),
            std::make_pair(static_cast<wchar_t>(251), static_cast<wchar_t>(9126)),
            std::make_pair(static_cast<wchar_t>(252), static_cast<wchar_t>(9131)),
            std::make_pair(static_cast<wchar_t>(253), static_cast<wchar_t>(9132)),
            std::make_pair(static_cast<wchar_t>(254), static_cast<wchar_t>(9133)),
            std::make_pair(static_cast<wchar_t>(180), static_cast<wchar_t>(215)),
            std::make_pair(static_cast<wchar_t>(184), static_cast<wchar_t>(247)),
            std::make_pair(static_cast<wchar_t>(216), static_cast<wchar_t>(172))
            };
        }
    //------------------------------------------------------------------
    wchar_t symbol_font_table::find(const wchar_t letter) const
        {
        const auto pos = m_symbol_table.find(letter);
        if (pos == m_symbol_table.cend() )
            { return letter; }
        return pos->second;
        }

    //------------------------------------------------------------------
    html_entity_table::html_entity_table()
        {
        // cppcheck-suppress useInitializationList
        m_table =
            {
            std::make_pair(std::wstring(L"apos"), static_cast<wchar_t>(L'\'')), // not standard, but common
            std::make_pair(std::wstring(L"gt"), static_cast<wchar_t>(L'>')),
            std::make_pair(std::wstring(L"lt"), static_cast<wchar_t>(L'<')),
            std::make_pair(std::wstring(L"amp"), static_cast<wchar_t>(L'&')),
            std::make_pair(std::wstring(L"quot"), static_cast<wchar_t>(L'"')),
            std::make_pair(std::wstring(L"nbsp"), static_cast<wchar_t>(L' ')),
            std::make_pair(std::wstring(L"iexcl"), static_cast<wchar_t>(161)),
            std::make_pair(std::wstring(L"cent"), static_cast<wchar_t>(162)),
            std::make_pair(std::wstring(L"pound"), static_cast<wchar_t>(163)),
            std::make_pair(std::wstring(L"curren"), static_cast<wchar_t>(164)),
            std::make_pair(std::wstring(L"yen"), static_cast<wchar_t>(165)),
            std::make_pair(std::wstring(L"brvbar"), static_cast<wchar_t>(166)),
            std::make_pair(std::wstring(L"sect"), static_cast<wchar_t>(167)),
            std::make_pair(std::wstring(L"uml"), static_cast<wchar_t>(168)),
            std::make_pair(std::wstring(L"copy"), static_cast<wchar_t>(169)),
            std::make_pair(std::wstring(L"ordf"), static_cast<wchar_t>(170)),
            std::make_pair(std::wstring(L"laquo"), static_cast<wchar_t>(171)),
            std::make_pair(std::wstring(L"not"), static_cast<wchar_t>(172)),
            std::make_pair(std::wstring(L"shy"), static_cast<wchar_t>(173)),
            std::make_pair(std::wstring(L"reg"), static_cast<wchar_t>(174)),
            std::make_pair(std::wstring(L"macr"), static_cast<wchar_t>(175)),
            std::make_pair(std::wstring(L"deg"), static_cast<wchar_t>(176)),
            std::make_pair(std::wstring(L"plusmn"), static_cast<wchar_t>(177)),
            std::make_pair(std::wstring(L"sup2"), static_cast<wchar_t>(178)),
            std::make_pair(std::wstring(L"sup3"), static_cast<wchar_t>(179)),
            std::make_pair(std::wstring(L"acute"), static_cast<wchar_t>(180)),
            std::make_pair(std::wstring(L"micro"), static_cast<wchar_t>(181)),
            std::make_pair(std::wstring(L"para"), static_cast<wchar_t>(182)),
            std::make_pair(std::wstring(L"middot"), static_cast<wchar_t>(183)),
            std::make_pair(std::wstring(L"mcedilicro"), static_cast<wchar_t>(184)),
            std::make_pair(std::wstring(L"sup1"), static_cast<wchar_t>(185)),
            std::make_pair(std::wstring(L"ordm"), static_cast<wchar_t>(186)),
            std::make_pair(std::wstring(L"raquo"), static_cast<wchar_t>(187)),
            std::make_pair(std::wstring(L"frac14"), static_cast<wchar_t>(188)),
            std::make_pair(std::wstring(L"texfrac12t"), static_cast<wchar_t>(189)),
            std::make_pair(std::wstring(L"frac34"), static_cast<wchar_t>(190)),
            std::make_pair(std::wstring(L"iquest"), static_cast<wchar_t>(191)),
            std::make_pair(std::wstring(L"Agrave"), static_cast<wchar_t>(192)),
            std::make_pair(std::wstring(L"Aacute"), static_cast<wchar_t>(193)),
            std::make_pair(std::wstring(L"Acirc"), static_cast<wchar_t>(194)),
            std::make_pair(std::wstring(L"Atilde"), static_cast<wchar_t>(195)),
            std::make_pair(std::wstring(L"Auml"), static_cast<wchar_t>(196)),
            std::make_pair(std::wstring(L"Aring"), static_cast<wchar_t>(197)),
            std::make_pair(std::wstring(L"AElig"), static_cast<wchar_t>(198)),
            std::make_pair(std::wstring(L"Ccedil"), static_cast<wchar_t>(199)),
            std::make_pair(std::wstring(L"Egrave"), static_cast<wchar_t>(200)),
            std::make_pair(std::wstring(L"Eacute"), static_cast<wchar_t>(201)),
            std::make_pair(std::wstring(L"Ecirc"), static_cast<wchar_t>(202)),
            std::make_pair(std::wstring(L"Euml"), static_cast<wchar_t>(203)),
            std::make_pair(std::wstring(L"Igrave"), static_cast<wchar_t>(204)),
            std::make_pair(std::wstring(L"Iacute"), static_cast<wchar_t>(205)),
            std::make_pair(std::wstring(L"Icirc"), static_cast<wchar_t>(206)),
            std::make_pair(std::wstring(L"Iuml"), static_cast<wchar_t>(207)),
            std::make_pair(std::wstring(L"ETH"), static_cast<wchar_t>(208)),
            std::make_pair(std::wstring(L"Ntilde"), static_cast<wchar_t>(209)),
            std::make_pair(std::wstring(L"Ograve"), static_cast<wchar_t>(210)),
            std::make_pair(std::wstring(L"Oacute"), static_cast<wchar_t>(211)),
            std::make_pair(std::wstring(L"Ocirc"), static_cast<wchar_t>(212)),
            std::make_pair(std::wstring(L"Otilde"), static_cast<wchar_t>(213)),
            std::make_pair(std::wstring(L"Ouml"), static_cast<wchar_t>(214)),
            std::make_pair(std::wstring(L"Oslash"), static_cast<wchar_t>(216)),
            std::make_pair(std::wstring(L"times"), static_cast<wchar_t>(215)),
            std::make_pair(std::wstring(L"Ugrave"), static_cast<wchar_t>(217)),
            std::make_pair(std::wstring(L"Uacute"), static_cast<wchar_t>(218)),
            std::make_pair(std::wstring(L"Ucirc"), static_cast<wchar_t>(219)),
            std::make_pair(std::wstring(L"Uuml"), static_cast<wchar_t>(220)),
            std::make_pair(std::wstring(L"Yacute"), static_cast<wchar_t>(221)),
            std::make_pair(std::wstring(L"THORN"), static_cast<wchar_t>(222)),
            std::make_pair(std::wstring(L"szlig"), static_cast<wchar_t>(223)),
            std::make_pair(std::wstring(L"agrave"), static_cast<wchar_t>(224)),
            std::make_pair(std::wstring(L"aacute"), static_cast<wchar_t>(225)),
            std::make_pair(std::wstring(L"acirc"), static_cast<wchar_t>(226)),
            std::make_pair(std::wstring(L"atilde"), static_cast<wchar_t>(227)),
            std::make_pair(std::wstring(L"auml"), static_cast<wchar_t>(228)),
            std::make_pair(std::wstring(L"aring"), static_cast<wchar_t>(229)),
            std::make_pair(std::wstring(L"aelig"), static_cast<wchar_t>(230)),
            std::make_pair(std::wstring(L"ccedil"), static_cast<wchar_t>(231)),
            std::make_pair(std::wstring(L"egrave"), static_cast<wchar_t>(232)),
            std::make_pair(std::wstring(L"eacute"), static_cast<wchar_t>(233)),
            std::make_pair(std::wstring(L"ecirc"), static_cast<wchar_t>(234)),
            std::make_pair(std::wstring(L"euml"), static_cast<wchar_t>(235)),
            std::make_pair(std::wstring(L"igrave"), static_cast<wchar_t>(236)),
            std::make_pair(std::wstring(L"iacute"), static_cast<wchar_t>(237)),
            std::make_pair(std::wstring(L"icirc"), static_cast<wchar_t>(238)),
            std::make_pair(std::wstring(L"iuml"), static_cast<wchar_t>(239)),
            std::make_pair(std::wstring(L"eth"), static_cast<wchar_t>(240)),
            std::make_pair(std::wstring(L"ntilde"), static_cast<wchar_t>(241)),
            std::make_pair(std::wstring(L"ograve"), static_cast<wchar_t>(242)),
            std::make_pair(std::wstring(L"oacute"), static_cast<wchar_t>(243)),
            std::make_pair(std::wstring(L"ocirc"), static_cast<wchar_t>(244)),
            std::make_pair(std::wstring(L"otilde"), static_cast<wchar_t>(245)),
            std::make_pair(std::wstring(L"ouml"), static_cast<wchar_t>(246)),
            std::make_pair(std::wstring(L"divide"), static_cast<wchar_t>(247)),
            std::make_pair(std::wstring(L"oslash"), static_cast<wchar_t>(248)),
            std::make_pair(std::wstring(L"ugrave"), static_cast<wchar_t>(249)),
            std::make_pair(std::wstring(L"uacute"), static_cast<wchar_t>(250)),
            std::make_pair(std::wstring(L"ucirc"), static_cast<wchar_t>(251)),
            std::make_pair(std::wstring(L"uuml"), static_cast<wchar_t>(252)),
            std::make_pair(std::wstring(L"yacute"), static_cast<wchar_t>(253)),
            std::make_pair(std::wstring(L"thorn"), static_cast<wchar_t>(254)),
            std::make_pair(std::wstring(L"yuml"), static_cast<wchar_t>(255)),
            std::make_pair(std::wstring(L"fnof"), static_cast<wchar_t>(402)),
            std::make_pair(std::wstring(L"Alpha"), static_cast<wchar_t>(913)),
            std::make_pair(std::wstring(L"Beta"), static_cast<wchar_t>(914)),
            std::make_pair(std::wstring(L"Gamma"), static_cast<wchar_t>(915)),
            std::make_pair(std::wstring(L"Delta"), static_cast<wchar_t>(916)),
            std::make_pair(std::wstring(L"Epsilon"), static_cast<wchar_t>(917)),
            std::make_pair(std::wstring(L"Zeta"), static_cast<wchar_t>(918)),
            std::make_pair(std::wstring(L"Eta"), static_cast<wchar_t>(919)),
            std::make_pair(std::wstring(L"Theta"), static_cast<wchar_t>(920)),
            std::make_pair(std::wstring(L"Iota"), static_cast<wchar_t>(921)),
            std::make_pair(std::wstring(L"Kappa"), static_cast<wchar_t>(922)),
            std::make_pair(std::wstring(L"Lambda"), static_cast<wchar_t>(923)),
            std::make_pair(std::wstring(L"Mu"), static_cast<wchar_t>(924)),
            std::make_pair(std::wstring(L"Nu"), static_cast<wchar_t>(925)),
            std::make_pair(std::wstring(L"Xi"), static_cast<wchar_t>(926)),
            std::make_pair(std::wstring(L"Omicron"), static_cast<wchar_t>(927)),
            std::make_pair(std::wstring(L"Pi"), static_cast<wchar_t>(928)),
            std::make_pair(std::wstring(L"Rho"), static_cast<wchar_t>(929)),
            std::make_pair(std::wstring(L"Sigma"), static_cast<wchar_t>(931)),
            std::make_pair(std::wstring(L"Tau"), static_cast<wchar_t>(932)),
            std::make_pair(std::wstring(L"Upsilon"), static_cast<wchar_t>(933)),
            std::make_pair(std::wstring(L"Phi"), static_cast<wchar_t>(934)),
            std::make_pair(std::wstring(L"Chi"), static_cast<wchar_t>(935)),
            std::make_pair(std::wstring(L"Psi"), static_cast<wchar_t>(936)),
            std::make_pair(std::wstring(L"Omega"), static_cast<wchar_t>(937)),
            std::make_pair(std::wstring(L"alpha"), static_cast<wchar_t>(945)),
            std::make_pair(std::wstring(L"beta"), static_cast<wchar_t>(946)),
            std::make_pair(std::wstring(L"gamma"), static_cast<wchar_t>(947)),
            std::make_pair(std::wstring(L"delta"), static_cast<wchar_t>(948)),
            std::make_pair(std::wstring(L"epsilon"), static_cast<wchar_t>(949)),
            std::make_pair(std::wstring(L"zeta"), static_cast<wchar_t>(950)),
            std::make_pair(std::wstring(L"eta"), static_cast<wchar_t>(951)),
            std::make_pair(std::wstring(L"theta"), static_cast<wchar_t>(952)),
            std::make_pair(std::wstring(L"iota"), static_cast<wchar_t>(953)),
            std::make_pair(std::wstring(L"kappa"), static_cast<wchar_t>(954)),
            std::make_pair(std::wstring(L"lambda"), static_cast<wchar_t>(955)),
            std::make_pair(std::wstring(L"mu"), static_cast<wchar_t>(956)),
            std::make_pair(std::wstring(L"nu"), static_cast<wchar_t>(957)),
            std::make_pair(std::wstring(L"xi"), static_cast<wchar_t>(958)),
            std::make_pair(std::wstring(L"omicron"), static_cast<wchar_t>(959)),
            std::make_pair(std::wstring(L"pi"), static_cast<wchar_t>(960)),
            std::make_pair(std::wstring(L"rho"), static_cast<wchar_t>(961)),
            std::make_pair(std::wstring(L"sigmaf"), static_cast<wchar_t>(962)),
            std::make_pair(std::wstring(L"sigma"), static_cast<wchar_t>(963)),
            std::make_pair(std::wstring(L"tau"), static_cast<wchar_t>(964)),
            std::make_pair(std::wstring(L"upsilon"), static_cast<wchar_t>(965)),
            std::make_pair(std::wstring(L"phi"), static_cast<wchar_t>(966)),
            std::make_pair(std::wstring(L"chi"), static_cast<wchar_t>(967)),
            std::make_pair(std::wstring(L"psi"), static_cast<wchar_t>(968)),
            std::make_pair(std::wstring(L"omega"), static_cast<wchar_t>(969)),
            std::make_pair(std::wstring(L"thetasym"), static_cast<wchar_t>(977)),
            std::make_pair(std::wstring(L"upsih"), static_cast<wchar_t>(978)),
            std::make_pair(std::wstring(L"piv"), static_cast<wchar_t>(982)),
            std::make_pair(std::wstring(L"bull"), static_cast<wchar_t>(8226)),
            std::make_pair(std::wstring(L"hellip"), static_cast<wchar_t>(8230)),
            std::make_pair(std::wstring(L"prime"), static_cast<wchar_t>(8242)),
            std::make_pair(std::wstring(L"Prime"), static_cast<wchar_t>(8243)),
            std::make_pair(std::wstring(L"oline"), static_cast<wchar_t>(8254)),
            std::make_pair(std::wstring(L"frasl"), static_cast<wchar_t>(8260)),
            std::make_pair(std::wstring(L"weierp"), static_cast<wchar_t>(8472)),
            std::make_pair(std::wstring(L"image"), static_cast<wchar_t>(8465)),
            std::make_pair(std::wstring(L"real"), static_cast<wchar_t>(8476)),
            std::make_pair(std::wstring(L"trade"), static_cast<wchar_t>(8482)),
            std::make_pair(std::wstring(L"alefsym"), static_cast<wchar_t>(8501)),
            std::make_pair(std::wstring(L"larr"), static_cast<wchar_t>(8592)),
            std::make_pair(std::wstring(L"uarr"), static_cast<wchar_t>(8593)),
            std::make_pair(std::wstring(L"rarr"), static_cast<wchar_t>(8594)),
            std::make_pair(std::wstring(L"darr"), static_cast<wchar_t>(8595)),
            std::make_pair(std::wstring(L"harr"), static_cast<wchar_t>(8596)),
            std::make_pair(std::wstring(L"crarr"), static_cast<wchar_t>(8629)),
            std::make_pair(std::wstring(L"lArr"), static_cast<wchar_t>(8656)),
            std::make_pair(std::wstring(L"uArr"), static_cast<wchar_t>(8657)),
            std::make_pair(std::wstring(L"rArr"), static_cast<wchar_t>(8658)),
            std::make_pair(std::wstring(L"dArr"), static_cast<wchar_t>(8659)),
            std::make_pair(std::wstring(L"hArr"), static_cast<wchar_t>(8660)),
            std::make_pair(std::wstring(L"forall"), static_cast<wchar_t>(8704)),
            std::make_pair(std::wstring(L"part"), static_cast<wchar_t>(8706)),
            std::make_pair(std::wstring(L"exist"), static_cast<wchar_t>(8707)),
            std::make_pair(std::wstring(L"empty"), static_cast<wchar_t>(8709)),
            std::make_pair(std::wstring(L"nabla"), static_cast<wchar_t>(8711)),
            std::make_pair(std::wstring(L"isin"), static_cast<wchar_t>(8712)),
            std::make_pair(std::wstring(L"notin"), static_cast<wchar_t>(8713)),
            std::make_pair(std::wstring(L"ni"), static_cast<wchar_t>(8715)),
            std::make_pair(std::wstring(L"prod"), static_cast<wchar_t>(8719)),
            std::make_pair(std::wstring(L"sum"), static_cast<wchar_t>(8721)),
            std::make_pair(std::wstring(L"minus"), static_cast<wchar_t>(8722)),
            std::make_pair(std::wstring(L"lowast"), static_cast<wchar_t>(8727)),
            std::make_pair(std::wstring(L"radic"), static_cast<wchar_t>(8730)),
            std::make_pair(std::wstring(L"prop"), static_cast<wchar_t>(8733)),
            std::make_pair(std::wstring(L"infin"), static_cast<wchar_t>(8734)),
            std::make_pair(std::wstring(L"ang"), static_cast<wchar_t>(8736)),
            std::make_pair(std::wstring(L"and"), static_cast<wchar_t>(8743)),
            std::make_pair(std::wstring(L"or"), static_cast<wchar_t>(8744)),
            std::make_pair(std::wstring(L"cap"), static_cast<wchar_t>(8745)),
            std::make_pair(std::wstring(L"cup"), static_cast<wchar_t>(8746)),
            std::make_pair(std::wstring(L"int"), static_cast<wchar_t>(8747)),
            std::make_pair(std::wstring(L"there4"), static_cast<wchar_t>(8756)),
            std::make_pair(std::wstring(L"sim"), static_cast<wchar_t>(8764)),
            std::make_pair(std::wstring(L"cong"), static_cast<wchar_t>(8773)),
            std::make_pair(std::wstring(L"asymp"), static_cast<wchar_t>(8776)),
            std::make_pair(std::wstring(L"ne"), static_cast<wchar_t>(8800)),
            std::make_pair(std::wstring(L"equiv"), static_cast<wchar_t>(8801)),
            std::make_pair(std::wstring(L"le"), static_cast<wchar_t>(8804)),
            std::make_pair(std::wstring(L"ge"), static_cast<wchar_t>(8805)),
            std::make_pair(std::wstring(L"sub"), static_cast<wchar_t>(8834)),
            std::make_pair(std::wstring(L"sup"), static_cast<wchar_t>(8835)),
            std::make_pair(std::wstring(L"nsub"), static_cast<wchar_t>(8836)),
            std::make_pair(std::wstring(L"sube"), static_cast<wchar_t>(8838)),
            std::make_pair(std::wstring(L"supe"), static_cast<wchar_t>(8839)),
            std::make_pair(std::wstring(L"oplus"), static_cast<wchar_t>(8853)),
            std::make_pair(std::wstring(L"otimes"), static_cast<wchar_t>(8855)),
            std::make_pair(std::wstring(L"perp"), static_cast<wchar_t>(8869)),
            std::make_pair(std::wstring(L"sdot"), static_cast<wchar_t>(8901)),
            std::make_pair(std::wstring(L"lceil"), static_cast<wchar_t>(8968)),
            std::make_pair(std::wstring(L"rceil"), static_cast<wchar_t>(8969)),
            std::make_pair(std::wstring(L"lfloor"), static_cast<wchar_t>(8970)),
            std::make_pair(std::wstring(L"rfloor"), static_cast<wchar_t>(8971)),
            std::make_pair(std::wstring(L"lang"), static_cast<wchar_t>(9001)),
            std::make_pair(std::wstring(L"rang"), static_cast<wchar_t>(9002)),
            std::make_pair(std::wstring(L"loz"), static_cast<wchar_t>(9674)),
            std::make_pair(std::wstring(L"spades"), static_cast<wchar_t>(9824)),
            std::make_pair(std::wstring(L"clubs"), static_cast<wchar_t>(9827)),
            std::make_pair(std::wstring(L"hearts"), static_cast<wchar_t>(9829)),
            std::make_pair(std::wstring(L"diams"), static_cast<wchar_t>(9830)),
            std::make_pair(std::wstring(L"OElig"), static_cast<wchar_t>(338)),
            std::make_pair(std::wstring(L"oelig"), static_cast<wchar_t>(339)),
            std::make_pair(std::wstring(L"Scaron"), static_cast<wchar_t>(352)),
            std::make_pair(std::wstring(L"scaron"), static_cast<wchar_t>(353)),
            std::make_pair(std::wstring(L"Yuml"), static_cast<wchar_t>(376)),
            std::make_pair(std::wstring(L"circ"), static_cast<wchar_t>(710)),
            std::make_pair(std::wstring(L"tilde"), static_cast<wchar_t>(732)),
            std::make_pair(std::wstring(L"ensp"), static_cast<wchar_t>(8194)),
            std::make_pair(std::wstring(L"emsp"), static_cast<wchar_t>(8195)),
            std::make_pair(std::wstring(L"thinsp"), static_cast<wchar_t>(8201)),
            std::make_pair(std::wstring(L"zwnj"), static_cast<wchar_t>(8204)),
            std::make_pair(std::wstring(L"zwj"), static_cast<wchar_t>(8205)),
            std::make_pair(std::wstring(L"lrm"), static_cast<wchar_t>(8206)),
            std::make_pair(std::wstring(L"rlm"), static_cast<wchar_t>(8207)),
            std::make_pair(std::wstring(L"ndash"), static_cast<wchar_t>(8211)),
            std::make_pair(std::wstring(L"mdash"), static_cast<wchar_t>(8212)),
            std::make_pair(std::wstring(L"lsquo"), static_cast<wchar_t>(8216)),
            std::make_pair(std::wstring(L"rsquo"), static_cast<wchar_t>(8217)),
            std::make_pair(std::wstring(L"sbquo"), static_cast<wchar_t>(8218)),
            std::make_pair(std::wstring(L"ldquo"), static_cast<wchar_t>(8220)),
            std::make_pair(std::wstring(L"rdquo"), static_cast<wchar_t>(8221)),
            std::make_pair(std::wstring(L"bdquo"), static_cast<wchar_t>(8222)),
            std::make_pair(std::wstring(L"dagger"), static_cast<wchar_t>(8224)),
            std::make_pair(std::wstring(L"Dagger"), static_cast<wchar_t>(8225)),
            std::make_pair(std::wstring(L"permil"), static_cast<wchar_t>(8240)),
            std::make_pair(std::wstring(L"lsaquo"), static_cast<wchar_t>(8249)),
            std::make_pair(std::wstring(L"rsaquo"), static_cast<wchar_t>(8250)),
            std::make_pair(std::wstring(L"euro"), static_cast<wchar_t>(8364))
            };
        }

    //------------------------------------------------------------------
    wchar_t html_entity_table::find(const wchar_t* html_entity, const size_t length) const
        {
        std::wstring cmpKey(html_entity, length);
        auto pos = m_table.find(cmpKey);
        // if not found case sensitively...
        if (pos == m_table.cend() )
            {
            // ...do a case insensitive search.
            std::transform(cmpKey.begin(), cmpKey.end(), cmpKey.begin(), std::towlower);
            pos = m_table.find(cmpKey);
            // if the character can't be converted, then return a question mark
            if (pos == m_table.cend() )
                { return L'?'; }
            }
        return pos->second;
        }

    //------------------------------------------------------------------
    const wchar_t* javascript_hyperlink_parse::operator()() noexcept
        {
        // if the end is null (should not happen) or if the current position
        // is null or at the terminator then we are done
        if (!m_js_text_end || !m_js_text_start || m_js_text_start[0] == 0)
            { return nullptr; }

        // jump over the previous link (and its trailing quote)
        m_js_text_start += (get_current_hyperlink_length() > 0) ?
                            get_current_hyperlink_length() + 1 : 0;
        if (m_js_text_start >= m_js_text_end)
            { return nullptr; }

        for (;;)
            {
            m_js_text_start = std::wcschr(m_js_text_start, L'"');
            if (m_js_text_start && m_js_text_start < m_js_text_end)
                {
                const wchar_t* endQuote = std::wcschr(++m_js_text_start, L'"');
                if (endQuote && endQuote < m_js_text_end)
                    {
                    m_current_hyperlink_length = (endQuote-m_js_text_start);
                    // see if the current link has a 3 or 4 character file
                    // extension on it--if not, this is not a link
                    if (m_current_hyperlink_length < 6  ||
                        (m_js_text_start[m_current_hyperlink_length-4] != L'.' &&
                        m_js_text_start[m_current_hyperlink_length-5] != L'.'))
                        {
                        m_current_hyperlink_length = 0;
                        m_js_text_start = ++endQuote;
                        continue;
                        }
                    // make sure this value is a possible link
                    size_t i = 0;
                    for (i = 0; i < m_current_hyperlink_length; ++i)
                        {
                        if (is_unsafe_uri_char(m_js_text_start[i]))
                            { break; }
                        }
                    // if we didn't make through each character then there must be
                    // a bad char in this string, so move on
                    if (i < m_current_hyperlink_length)
                        {
                        m_current_hyperlink_length = 0;
                        m_js_text_start = ++endQuote;
                        continue;
                        }
                    return m_js_text_start;
                    }
                else
                    { return nullptr; }
                }
            else
                { return nullptr; }
            }
        }

    //------------------------------------------------------------------
    const wchar_t* html_image_parse::operator()()
        {
        static const std::wstring HTML_IMAGE(L"img");
        // reset
        m_current_hyperlink_length = 0;

        if (!m_html_text || m_html_text[0] == 0)
            { return nullptr; }

        while (m_html_text)
            {
            m_html_text = lily_of_the_valley::html_extract_text::find_element(m_html_text, m_html_text_end, HTML_IMAGE.c_str(), HTML_IMAGE.length());
            if (m_html_text)
                {
                const auto [imageSrc, imageLength] = lily_of_the_valley::html_extract_text::read_attribute(m_html_text, L"src", 3, false, true);
                if (imageSrc)
                    {
                    m_html_text = imageSrc;
                    m_current_hyperlink_length = imageLength;
                    return m_html_text;
                    }
                // no src in this anchor, so go to next one
                else
                    {
                    m_html_text += HTML_IMAGE.length()+1;
                    continue;
                    }
                }
            else
                { return m_html_text = nullptr; }
            }
        return m_html_text = nullptr;
        }

    //------------------------------------------------------------------
    html_hyperlink_parse::html_hyperlink_parse(const wchar_t* html_text, const size_t length) noexcept :
                m_html_text(html_text), m_html_text_end(html_text+length)
        {
        // see if there is a base url that should be used as an alternative that the client should use instead
        const wchar_t* headStart = string_util::stristr<wchar_t>(m_html_text, L"<head");
        if (!headStart)
            { return; }
        const wchar_t* base = string_util::stristr<wchar_t>(headStart, L"<base");
        if (!base)
            { return; }
        base = string_util::stristr<wchar_t>(base, L"href=");
        if (!base)
            { return; }
        const wchar_t firstLinkChar = base[5];
        base += 6;
        // eat any whitespace after href=
        for (;;)
            {
            if (!std::iswspace(base[0]) || base[0] == 0)
                { break; }
            ++base;
            }
        if (base[0] == 0)
            { return; }
        // look for actual link
        const wchar_t* endQuote = nullptr;
        if (firstLinkChar == L'"' ||
            firstLinkChar == L'\'')
            { endQuote = string_util::strcspn_pointer<wchar_t>(base, L"\"\'", 2); }
        // if hack author forgot to quote the link, then look for matching space
        else
            {
            --base;
            endQuote = string_util::strcspn_pointer<wchar_t>(base, L" \r\n\t>", 5);
            }

        // if src is malformed, then go to next one
        if (!endQuote)
            { return; }
        m_base = base;
        m_base_length = (endQuote-base);
        }

    //------------------------------------------------------------------
    const wchar_t* html_hyperlink_parse::operator()()
        {
        static const std::wstring HTML_META(L"meta");
        static const std::wstring HTML_IFRAME(L"iframe");
        static const std::wstring HTML_FRAME(L"frame");
        static const std::wstring HTML_SCRIPT(L"script");
        static const std::wstring HTML_SCRIPT_END(L"</script>");
        static const std::wstring HTML_IMAGE(L"img");
        // if we are in an embedded script block, then continue parsing the
        // links out of that instead of using the regular parser
        if (m_inside_of_script_section)
            {
            const wchar_t* currentLink = m_javascript_hyperlink_parse();
            if (currentLink)
                {
                m_current_link_is_image = false;
                m_current_link_is_javascript = false;
                m_current_hyperlink_length = m_javascript_hyperlink_parse.get_current_hyperlink_length();
                return currentLink;
                }
            }
        // reset everything
        m_current_hyperlink_length = 0;
        m_current_link_is_image = false;
        m_current_link_is_javascript = false;
        m_inside_of_script_section = false;

        if (!m_html_text || m_html_text[0] == 0)
            { return nullptr; }

        for (;;)
            {
            m_html_text = std::wcschr(m_html_text, L'<');
            if (m_html_text && m_html_text+1 < m_html_text_end)
                {
                // don't bother with termination element
                if (m_html_text[1] == L'/')
                    {
                    ++m_html_text;
                    continue;
                    }
                m_current_link_is_image = lily_of_the_valley::html_extract_text::compare_element(m_html_text+1, HTML_IMAGE.c_str(), HTML_IMAGE.length(), false);
                m_inside_of_script_section = m_current_link_is_javascript = lily_of_the_valley::html_extract_text::compare_element(m_html_text+1, HTML_SCRIPT.c_str(), HTML_SCRIPT.length(), false);
                if (m_inside_of_script_section)
                    {
                    const wchar_t* endAngle = lily_of_the_valley::html_extract_text::find_close_tag(m_html_text);
                    const wchar_t* endOfScriptSection = string_util::stristr<wchar_t>(m_html_text, HTML_SCRIPT_END.c_str());
                    if (endAngle && (endAngle < m_html_text_end) &&
                        endOfScriptSection && (endOfScriptSection < m_html_text_end))
                        { m_javascript_hyperlink_parse.set(endAngle, endOfScriptSection-endAngle); }
                    }

                // see if it is an IMG, Frame (sometimes they have a SRC to another HTML page), or JS link
                if ((m_include_image_links && m_current_link_is_image) ||
                    m_current_link_is_javascript  ||
                    lily_of_the_valley::html_extract_text::compare_element(m_html_text+1, HTML_FRAME.c_str(), HTML_FRAME.length(), false)  ||
                    lily_of_the_valley::html_extract_text::compare_element(m_html_text+1, HTML_IFRAME.c_str(), HTML_IFRAME.length(), false))
                    {
                    m_html_text += 4;
                    const auto [imageSrc, imageLengh] = lily_of_the_valley::html_extract_text::read_attribute(m_html_text, L"src", 3, false, true);
                    if (imageSrc)
                        {
                        m_html_text = imageSrc;
                        m_current_hyperlink_length = imageLengh;
                        return m_html_text;
                        }
                    // if we are currently in a SCRIPT section (that didn't have a link in it),
                    // then start parsing that section instead
                    else if (m_inside_of_script_section)
                        {
                        const wchar_t* currentLink = m_javascript_hyperlink_parse();
                        if (currentLink)
                            {
                            m_current_link_is_image = false;
                            m_current_link_is_javascript = false;
                            m_current_hyperlink_length = m_javascript_hyperlink_parse.get_current_hyperlink_length();
                            return currentLink;
                            }
                        else
                            {
                            m_inside_of_script_section = false;
                            continue;
                            }
                        }
                    // no src in this anchor, so go to next one
                    else
                        { continue; }
                    }
                // ...or it is an anchor link
                else if (lily_of_the_valley::html_extract_text::compare_element(m_html_text+1, L"a", 1, false) ||
                    lily_of_the_valley::html_extract_text::compare_element(m_html_text+1, L"link", 4, false) ||
                    lily_of_the_valley::html_extract_text::compare_element(m_html_text+1, L"area", 4, false) )
                    {
                    ++m_html_text; // skip the <
                    const auto [attribText, attribLength] = lily_of_the_valley::html_extract_text::read_attribute(m_html_text, L"href", 4, false, true);
                    if (attribText && attribLength > 0)
                        {
                        m_html_text = attribText;
                        m_current_hyperlink_length = attribLength;
                        return m_html_text;
                        }
                    //no href in this anchor, so go to next one
                    else
                        { continue; }
                    }
                // ...or a redirect in the HTTP meta section
                else if (lily_of_the_valley::html_extract_text::compare_element(m_html_text+1, HTML_META.c_str(), HTML_META.size(), false) )
                    {
                    m_html_text += HTML_META.size() + 1;
                    const std::wstring httpEquiv = lily_of_the_valley::html_extract_text::read_attribute_as_string(m_html_text,
                        L"http-equiv", 10, false);
                    if (string_util::stricmp(httpEquiv.c_str(), L"refresh") == 0)
                        {
                        const wchar_t* url = lily_of_the_valley::html_extract_text::find_tag(m_html_text, L"url=", 4, true);
                        if (url && (url < m_html_text_end))
                            {
                            m_html_text = url+4;
                            if (m_html_text[0] == 0)
                                { return nullptr; }
                            // eat up any whitespace or single quotes
                            for (;;)
                                {
                                if (m_html_text[0] == 0 ||
                                    (!std::iswspace(m_html_text[0]) &&
                                    m_html_text[0] != L'\''))
                                    { break; }
                                ++m_html_text;
                                }
                            if (m_html_text[0] == 0)
                                { return nullptr; }
                            const wchar_t* endOfTag = string_util::strcspn_pointer<wchar_t>(m_html_text, L"'\">", 3);
                            // if link is malformed then go to next one
                            if (endOfTag == nullptr || (endOfTag > m_html_text_end))
                                { continue; }
                            m_current_hyperlink_length = endOfTag - m_html_text;
                            return m_html_text;
                            }
                        }
                    }
                else
                    {
                    ++m_html_text;
                    continue;
                    }
                }
            else
                { return nullptr; }
            }
        }

    //------------------------------------------------------------------
    html_url_format::html_url_format(const wchar_t* root_url) : m_last_slash(std::wstring::npos),
                                                    m_query(std::wstring::npos)
        {
        if (root_url)
            {
            m_root_url = root_url;
            m_current_url = root_url;
            }
        m_last_slash = find_last_directory(m_root_url, m_query);
        parse_domain(m_root_url, m_root_full_domain, m_root_domain, m_root_subdomain);
        // parse this as the current URL too until the () function is called by the client
        parse_domain(m_current_url, m_current_full_domain, m_current_domain, m_current_subdomain);
        if (has_query())
            { m_image_name = parse_image_name_from_url(m_root_url.c_str()); }
        }

    //------------------------------------------------------------------
    const wchar_t* html_url_format::operator()(const wchar_t* path, size_t text_length,
                                               const bool is_image /*= false*/)
        {
        if (!path || text_length == 0)
            { return nullptr; }
        // see if it's a valid URL already
        if (is_absolute_url(path) )
            {
            m_current_url.assign(path, text_length);
            }
        // first see if it is a queried link first
        else if (path[0] == L'?' && (m_query != std::wstring::npos))
            {
            m_current_url.assign(m_root_url, 0, m_query);
            m_current_url.append(path, text_length);
            }
        // or a link meant for the root of the full domain
        else if (path[0] == L'/')
            {
            m_current_url.assign(m_root_full_domain);
            if (m_current_url.length() > 1 && m_current_url[m_current_url.length()-1] != L'/')
                { m_current_url += (L'/'); }
            m_current_url.append(path+1, text_length-1);
            }
        // or if "./" is in front then strip it because it is redundant
        else if (string_util::strnicmp<wchar_t>(path, L"./", 2) == 0)
            {
            m_current_url.assign(m_root_url, 0, m_last_slash+1);
            m_current_url.append(path+2, text_length-2);
            }
        // or a relative link that goes up a few folders
        else if (std::wcsncmp(path, L"../", 3) == 0)
            {
            size_t folderLevelsToGoUp = 1;
            const wchar_t* start = path+3;
            for (;;)
                {
                if (std::wcsncmp(start, L"../", 3) == 0)
                    { start = path + (3*(++folderLevelsToGoUp)); }
                else
                    { break; }
                }
            size_t lastSlash = m_last_slash-1;
            while (folderLevelsToGoUp-- > 0)
                {
                const size_t currentLastSlash = m_root_url.rfind(L'/', lastSlash-1);
                /* just in case there aren't enough directories in the root url then
                   just piece together what you can later.*/
                if (currentLastSlash == std::wstring::npos)
                    { break; }
                lastSlash = currentLastSlash;
                }
            /* make sure we didn't go all the way back to the protocol (e.g., "http://").
               If so, the move back up a folder. This can happen if there is an incorrect "../" in front
               of this link.*/
            if ((lastSlash < m_root_url.length()-2 && lastSlash > 0) &&
                is_either<wchar_t>(L'/', m_root_url[lastSlash-1], m_root_url[lastSlash+1]))
                { lastSlash = m_root_url.find(L'/', lastSlash+2); }
            if (lastSlash == std::wstring::npos)
                {m_current_url = m_root_url; }
            else
                { m_current_url.assign(m_root_url, 0, lastSlash+1); }
            m_current_url.append(start, text_length-(start-path));
            }
        // ...or just a regular link
        else
            {
            m_current_url.assign(m_root_url, 0, m_last_slash+1);
            m_current_url.append(path, text_length);
            }

        // if this is a bookmark then chop it off
        const size_t bookmark = m_current_url.rfind(L'#');
        if (bookmark != std::wstring::npos)
            { m_current_url.erase(bookmark); }

        /* sometimes with PHP the IMG SRC is just the folder path and you
           need to append the "image" value from the page's url*/
        if (is_image && m_current_url.length() > 1 && m_current_url[m_current_url.length()-1] == L'/')
            { m_current_url += m_image_name; }

        // now get the domain information about this URL
        parse_domain(m_current_url, m_current_full_domain, m_current_domain, m_current_subdomain);

        // encode any spaces in the URL
        string_util::replace_all(m_current_url, L" ", 1, L"%20");
        return m_current_url.c_str();
        }

    //------------------------------------------------------------------
    std::wstring html_url_format::get_directory_path()
        {
        size_t domainDirectoryPath = 0;
        if (string_util::strnicmp<wchar_t>(m_current_url.c_str(), L"http://", 7) == 0)
            { domainDirectoryPath = 7; }
        else if (string_util::strnicmp<wchar_t>(m_current_url.c_str(), L"ftp://", 6) == 0)
            { domainDirectoryPath = 6; }
        else
            { domainDirectoryPath = 0; }

        size_t query{ 0 };
        const size_t last_slash = find_last_directory(m_current_url, query);

        return m_current_url.substr(domainDirectoryPath, (last_slash-domainDirectoryPath));
        }

    //------------------------------------------------------------------
    std::wstring html_url_format::parse_image_name_from_url(const wchar_t* url)
        {
        static const std::wstring PHP_IMAGE(L"image=");
        std::wstring image_name;
        if (url == nullptr || url[0] == 0)
            { return image_name; }
        const wchar_t* start = std::wcschr(url, L'?');
        if (start == nullptr)
            { return image_name; }
        start = string_util::stristr(start, PHP_IMAGE.c_str());
        if (start == nullptr)
            { return image_name; }
        start += PHP_IMAGE.length();
        // see if it is the in front of another command
        const wchar_t* end = std::wcschr(start, L'&');
        if (end == nullptr)
            { image_name.assign(start); }
        else
            { image_name.assign(start, end-start); }
        return image_name; 
        }

    //------------------------------------------------------------------
    std::wstring html_url_format::parse_top_level_domain_from_url(const wchar_t* url)
        {
        static const std::wstring WWW(L"www.");
        std::wstring tld;
        if (url == nullptr || url[0] == 0)
            { return tld; }
        // move to after the "www." or (if not there) the start of the url
        const wchar_t* start = string_util::stristr(url, WWW.c_str());
        if (start)
            { start += WWW.length(); }
        else
            { start = url; }
        start = std::wcschr(start, L'.');
        if (start == nullptr || start[1] == 0)
            { return tld; }
        const wchar_t* end = string_util::strcspn_pointer(++start, L"/?", 2);
        if (end == nullptr)
            { tld.assign(start); }
        else
            { tld.assign(start, end-start); }
        return tld;
        }

    //------------------------------------------------------------------
    bool html_url_format::is_url_top_level_domain(const wchar_t* url) noexcept
        {
        if (url == nullptr || url[0] == 0)
            { return false; }
        // move to after the protocol's "//" or (if not there) the start of the url
        const wchar_t* start = string_util::stristr(url, L"//");
        if (start)
            { start += 2; }
        else
            { start = url; }
        // if no more slashes in the URL or if that last slash is the last character
        // then this must be just a domain
        start = std::wcschr(start, L'/');
        if (start == nullptr || start[1] == 0)
            { return true; }
        else
            { return false; }
        }

    //------------------------------------------------------------------
    size_t html_url_format::find_last_directory(std::wstring& url, size_t& query_postion)
        {
        // if this is a queried page then see where the command is at
        query_postion = url.rfind(L'?');
        size_t last_slash = url.rfind(L'/');

        // if this page is queried then backtrack to the '/' right before the query
        if ((query_postion != std::wstring::npos) && (last_slash != std::wstring::npos) &&
            (query_postion > 0) && (last_slash > 0) &&
            (last_slash > query_postion))
            { last_slash = url.rfind(L'/', query_postion-1); }
        // see if the slash is just the one after "http:/" or if there no none at all
        if ((last_slash != std::wstring::npos &&
            (last_slash > 0) &&
            (url[last_slash-1] == L'/')) ||
            last_slash == std::wstring::npos)
            {
            // e.g., http://www.website.com/
            url += L'/';
            last_slash = url.length()-1;
            }
        return last_slash;
        }

    //------------------------------------------------------------------
    void html_url_format::parse_domain(const std::wstring& url, std::wstring& full_domain,
                            std::wstring& domain, std::wstring& subdomain)
        {
        full_domain.clear();
        domain.clear();
        size_t startIndex = 0;
        if (string_util::strnicmp<wchar_t>(url.c_str(), L"http://", 7) == 0)
            { startIndex = 7; }
        else if (string_util::strnicmp<wchar_t>(url.c_str(), L"https://", 8) == 0)
            { startIndex = 8; }
        else if (string_util::strnicmp<wchar_t>(url.c_str(), L"ftp://", 6) == 0)
            { startIndex = 6; }
        else if (string_util::strnicmp<wchar_t>(url.c_str(), L"ftps://", 7) == 0)
            { startIndex = 7; }
        else
            { startIndex = 0; }
        const size_t lastSlash = url.find(L'/', startIndex);
        if (lastSlash == std::wstring::npos)
            { full_domain = url; }
        else
            { full_domain = url.substr(0, lastSlash); }

        // http://www.sales.mycompany.com: go to dot in front of ".com"
        size_t dot = full_domain.rfind(L'.', lastSlash);
        if (dot == std::wstring::npos || dot == 0)
            { return; }
        // now go back one more dot to go after the "www." prefix or subdomain
        dot = full_domain.rfind(L'.', --dot);
        // skip the dot
        if (dot != std::wstring::npos)
            { ++dot; }
        // if there is no "www." prefix then go the end of the protocol
        else
            { dot = startIndex; }
        subdomain = domain = full_domain.substr(dot, lastSlash-dot);
        // now the subdomain.  If no subdomain, then this will be the same as the domain.
        if (dot != startIndex && dot > 2)
            {
            dot = full_domain.rfind(L'.', dot-2);
            if (dot != std::wstring::npos)
                {
                ++dot;
                subdomain = full_domain.substr(dot, lastSlash-dot);
                }
            }
        }
    }
