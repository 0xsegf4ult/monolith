#pragma once

#include <lib/types.hpp>

extern "C" void __assertion_fail_handler(const char*);

constexpr uint32_t pow10_uint32_lut[]
{
        1,
        10,
        100,    
        1000,   
        10000,  
        100000,
        1000000,
        10000000,
        100000000,
        1000000000      
};

constexpr uint64_t pow10_uint64_lut[]
{
        1,      
        10,     
        100,
        1000,   
        10000,
        100000,
        1000000,
        10000000,
        100000000,
        1000000000,
        10000000000,
        100000000000,
        1000000000000,
        10000000000000,
        100000000000000,
        1000000000000000,
        10000000000000000,
        100000000000000000,
        1000000000000000000,
        10000000000000000000u
};

constexpr int count_digits_dec(uint32_t n)
{
        if(n < 10)      
                return 1;

        const int t = (32 - __builtin_clz(n)) * 1233 >> 12;
        return t - (n < pow10_uint32_lut[t]) + 1;
}

constexpr int count_digits_dec(uint64_t n)
{
        if(n <= u32_max)
                return count_digits_dec(static_cast<uint32_t>(n));

        const int t = (64 - __builtin_clzll(n)) * 1233 >> 12;
        return t - (n < pow10_uint64_lut[t]) + 1;
}

template <typename T>
constexpr int count_digits_hex(T n)
{
        int digits = 1;
        while((n >>= 4u) != 0)
                ++digits;
        return digits;
}

constexpr int count_digits_bin(const uint32_t n)
{
        return (n < 2) ? 1 : (32 - __builtin_clz(n));
}

constexpr int count_digits_bin(const uint64_t n)
{
        return (n < 2) ? 1 : (64 - __builtin_clzll(n));
}

constexpr const char* digits_hex_lowercase = "0123456789abcdef";

template <typename T>
constexpr void convert_dec(char* dst, T value)
{
        do
        {
                const T v = value;
                value /= 10;
                *(--dst) = static_cast<char>('0' + (v - (value * 10)));
        } while(value);
}

template <typename T>
constexpr void convert_hex(char* dst, T value)
{
        do
        {
                const T v = value;
                value >>= 4u;
                *(--dst) = digits_hex_lowercase[v - (value << 4u)];
        } while(value);
}

template <typename T>
constexpr void convert_bin(char* dst, T value)
{
        do
        {
                const T v = value;
                value >>= 1u;
                *(--dst) = static_cast<char>('0' + (v - (value << 1u)));
        } while(value);
}

struct fmt_arg_format
{
public:
        using iterator = char*;
        using const_iterator = const char*;

        constexpr fmt_arg_format(string_view& fmt, const int arg_count)
	{
		const_iterator it = fmt.cbegin();

                if(*it != '{')
                {
                        __assertion_fail_handler("failed to parse format string: expected character {");
                        return;
                }

                ++it;

                if(*it == ':' && *(it + 1) != '}')
                {
                        ++it;

                        if(*it != '}')
                        {
                                switch(*it++)
                                {
                                case 'c':
                                        m_type = Type::Char;
                                        break;
                                case 'd':
                                        m_type = Type::IntegerDec;
                                        break;
                                case 'x':
                                        m_type = Type::IntegerHex;
                                        break;
                                case 'b':
                                        m_type = Type::IntegerBin;
                                        break;
                                case 'p':
                                        m_type = Type::Pointer;
                                        break;
                                case 's':
                                        m_type = Type::String;
                                        break;
                                default:
                                        m_type = Type::Invalid;
                                        break;
                                }

                                if(m_type == Type::Invalid)
                                        __assertion_fail_handler("failed to parse format string: invalid type specifier");
                        }
                }
			if(!(it < fmt.cend() && *it++ == '}'))
                		__assertion_fail_handler("failed to parse format string: unterminated format argument specifier");

                fmt.remove_prefix(it - fmt.cbegin());
        }

	constexpr int index() const { return static_cast<int>(m_index); }

        template <typename T>
        void format_integer(iterator& it, const_iterator end, const T value)
        {
                if(m_type == Type::None || m_type == Type::IntegerDec)
                {
                        const auto digits = count_digits_dec(value);
                        it += digits;
                        convert_dec(it, value);
                }
                else if(m_type == Type::IntegerHex)
                {
                        const auto digits = count_digits_hex(value);
                        *it++ = '0';
                        *it++ = 'x';
                        it += digits;
                        convert_hex(it, value);
                }
                else if(m_type == Type::IntegerBin)
                {
                        const auto digits = count_digits_bin(value);
                        *it++ = '0';
                        *it++ = 'b';
                        it += digits;
                        convert_bin(it, value);
                }
        }
	
	void format_pointer(iterator& it, const_iterator end, const uintptr_t value)
	{
		auto ivalue = static_cast<uint64_t>(value);
                const auto digits = count_digits_hex(ivalue);
                *it++ = '0';
                *it++ = 'x';
                it += digits;
                convert_hex<uint64_t>(it, ivalue);
	}

	void format_string(iterator& it, const_iterator end, const string_view& str)
	{
		auto count = str.size();
		const auto* data = str.data();
		while((count--) > 0)
			*it++ = *data++;
	}
private:
	enum class Type
        {
                None,
                Char,
                IntegerDec,
                IntegerHex,
                IntegerBin,
                Pointer,
                String,
                Invalid
        };

        Type m_type{Type::None};
        int8_t m_index{-1};
};

struct fmt_argument
{
public:
        using iterator = char*;
        using const_iterator = const char*;

        using Format = fmt_arg_format;

        constexpr fmt_argument() = delete;
        constexpr fmt_argument(const bool value) : v_bool{value}, type_id{TypeID::Bool} {}
        constexpr fmt_argument(const char value) : v_char{value}, type_id{TypeID::Char} {}
        constexpr fmt_argument(const int32_t value) : v_int32{value}, type_id{TypeID::Int32} {}
        constexpr fmt_argument(const uint32_t value) : v_uint32{value}, type_id{TypeID::UInt32} {}
        constexpr fmt_argument(const int64_t value) : v_int64{value}, type_id{TypeID::Int64} {}
        constexpr fmt_argument(const uint64_t value) : v_uint64{value}, type_id{TypeID::UInt64} {}
        constexpr fmt_argument(const void* value) : v_pointer{reinterpret_cast<uintptr_t>(value)}, type_id{TypeID::Pointer} {}
        constexpr fmt_argument(const string_view value) : v_string{value}, type_id{TypeID::String} {}

        constexpr void format(string_span& dst, Format& format) const
	{
		iterator it = dst.begin();

                switch(type_id)
                {
		case TypeID::Char:
			*it++ = v_char;
			break;
                case TypeID::UInt32:
                        format.format_integer(it, dst.end(), v_uint32);
                        break;
                case TypeID::UInt64:
                        format.format_integer(it, dst.end(), v_uint64);
                        break;
                case TypeID::Pointer:
                        format.format_pointer(it, dst.end(), v_pointer);
                        break;
                case TypeID::String:
                        format.format_string(it, dst.end(), v_string);
                        break;
                default:
                        __assertion_fail_handler("unhandled typeID");
                }

                dst.remove_prefix(it - dst.begin());
        }
private:
        union
        {
                bool v_bool;
                char v_char;
                int32_t v_int32;
                uint32_t v_uint32;
                int64_t v_int64;
                uint64_t v_uint64;
                uintptr_t v_pointer;
                string_view v_string;
        };

        enum class TypeID
        {
                Bool = 0,
                Char,
                Int32,
                UInt32,
                Int64,
                UInt64,
                Pointer,
                String
        } type_id;
};

inline constexpr fmt_argument make_argument(const bool arg)
{
        return arg;
}

inline constexpr fmt_argument make_argument(const char arg)
{
        return arg;
}

inline constexpr fmt_argument make_argument(const int8_t arg)
{
        return static_cast<int32_t>(arg);
}

inline constexpr fmt_argument make_argument(const uint8_t arg)
{
        return static_cast<uint32_t>(arg);
}

inline constexpr fmt_argument make_argument(const int16_t arg)
{
        return static_cast<int32_t>(arg);
}

inline constexpr fmt_argument make_argument(const uint16_t arg)
{
        return static_cast<uint32_t>(arg);
}

inline constexpr fmt_argument make_argument(const int arg)
{
        return static_cast<int32_t>(arg);
}

inline constexpr fmt_argument make_argument(const unsigned int arg)
{
        return static_cast<uint32_t>(arg);
}

inline constexpr fmt_argument make_argument(const int64_t arg)
{
        return arg;
}

inline constexpr fmt_argument make_argument(const uint64_t arg)
{
        return arg;
}

inline constexpr fmt_argument make_argument(void* arg)
{
        return arg;
}

inline constexpr fmt_argument make_argument(const void* arg)
{
        return arg;
}

inline constexpr fmt_argument make_argument(const char* arg)
{
        return string_view(arg);
}

constexpr void parse_fmt_string(string_span& out_buffer, string_view& fmt)
{
	auto* out_it = out_buffer.begin();
        const auto* fmt_it = fmt.cbegin();

        while(fmt_it < fmt.cend() && out_it < out_buffer.end())
        {
                if(*fmt_it == '{')
                {
                        if(*(fmt_it + 1) == '{')
                        {
                                ++fmt_it;
                                *out_it++ = *fmt_it++;
                        }
                        else
                                break;
                }
                else if(*fmt_it == '}')
                {
                        if(*(fmt_it + 1) != '}')
                        {
                                __assertion_fail_handler("*(fmt_it + 1) == '}'");
                                return;
                        }

                        ++fmt_it;
                        *out_it++ = *fmt_it++;
                }
                else
                {
                        *out_it++ = *fmt_it++;
                }
        }

        out_buffer.remove_prefix(out_it - out_buffer.begin());
        fmt.remove_prefix(fmt_it - fmt.cbegin());
}

constexpr void fmt_process(string_span& out_buffer, string_view& fmt, const fmt_argument* const args, const int arg_count)
{
	int arg_seq_index = 0;

        parse_fmt_string(out_buffer, fmt);

        while(!fmt.empty())
        {
                fmt_arg_format format(fmt, arg_count);
                int arg_index = format.index();

                if(arg_index < 0)
                {
                        if(arg_seq_index >= arg_count)
                        {
                                __assertion_fail_handler("arg_seq_index < arg_count");
                                return;
                        }
                        arg_index = arg_seq_index++;
                }
                args[arg_index].format(out_buffer, format);

                parse_fmt_string(out_buffer, fmt);
        }
}

template <typename... Args>
constexpr void format_to(string_span out_buffer, string_view fmt)
{
	parse_fmt_string(out_buffer, fmt);
	out_buffer[0] = '\0';
}

template <typename... Args>
constexpr void format_to(string_span out_buffer, string_view fmt, Args&&... args)
{
	const fmt_argument arguments[sizeof...(Args)]{make_argument(args)...};
	fmt_process(out_buffer, fmt, arguments, static_cast<int>(sizeof...(Args)));
	out_buffer[0] = '\0';
}
