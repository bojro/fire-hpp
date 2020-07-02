
/*
    Copyright Kristjan Kongas 2020

    Boost Software License - Version 1.0 - August 17th, 2003

    Permission is hereby granted, free of charge, to any person or organization
    obtaining a copy of the software and accompanying documentation covered by
    this license (the "Software") to use, reproduce, display, distribute,
    execute, and transmit the Software, and to prepare derivative works of the
    Software, and to permit third-parties to whom the Software is furnished to
    do so, all subject to the following:

    The copyright notices in the Software and this entire statement, including
    the above license grant, this restriction and the following disclaimer,
    must be included in all copies of the Software, in whole or in part, and
    all derivative works of the Software, unless such copies or derivative
    works are solely in the form of machine-executable object code generated by
    a source language processor.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
    SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
    FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
    ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#ifndef __FIRE_HPP__
#define __FIRE_HPP__

#include <string>
#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cassert>
#include <cstdlib>
#include <cstddef>
#include <algorithm>
#include <type_traits>
#include <limits>


namespace fire {
    constexpr int _failure_code = 1;

    template<typename R, typename ... Types>
    constexpr size_t _get_argument_count(R(*)(Types ...)) { return sizeof...(Types); }

    inline void _instant_assert(bool pass, const std::string &msg, bool programmer_side = true);
    inline int count_hyphens(const std::string &s);

    template <typename T>
    class optional {
        T _value = T();
        bool _exists = false;

    public:
        optional() = default;
        optional(T __value): _value(std::move(__value)), _exists(true) {}
        optional<T>& operator=(const T& __value) { _value = __value; _exists = true; return *this; }
        bool operator==(const optional<T>& other) { return _exists == other._exists && _value == other._value; }
        explicit operator bool() const { return _exists; }
        bool has_value() const { return _exists; }
        T value_or(const T& def) const { return _exists ? _value : def; }
        T value() const { _instant_assert(_exists, "accessing unassigned optional"); return _value; }
    };

    class identifier {
        optional<std::string> _short_name, _long_name;
        optional<int> _pos;
        bool _vector = false;

        std::string _help, _longer;

        inline static void _check_name(const std::string &name);
    public:
        inline static std::string prepend_hyphens(const std::string &name);

        inline identifier(): _help("..."), _longer("...") { _vector = true; };
        inline identifier(const char *name_c);
        inline identifier(int pos): _pos(pos), _help("<" + std::to_string(pos) + ">"), _longer(_help) {}
        inline identifier(const char *name_c1, const char *name_c2);
        inline identifier(int pos, const char *help): _pos(pos), _help(help), _longer(help) { _check_name(_help); }
        inline identifier(const char *help, int pos): _pos(pos), _help(help), _longer(help) { _check_name(_help); }

        inline bool operator<(const identifier &other) const;
        inline bool overlaps(const identifier &other) const;
        inline bool contains(const std::string &name) const;
        inline bool contains(int pos) const;
        inline std::string help() const { return _help; }
        inline std::string longer() const { return _longer; }
        inline optional<int> get_pos() const { return _pos; }
        inline bool vector() const { return _vector; };
    };

    template<typename ORDER, typename VALUE>
    class _first {
        ORDER _order;
        VALUE _value;
        bool _empty = true;

    public:
        void set(const ORDER &__order, const VALUE &__value);
        const VALUE & get() const;
        bool empty() const { return _empty; }
    };

    class _matcher {
        std::string _executable;
        std::vector<std::string> _positional;
        std::vector<std::pair<std::string, optional<std::string>>> _named;
        std::vector<identifier> _queried;
        _first<identifier, std::string> _deferred_error;
        int _main_argc = 0;
        bool _space_assignment = false;
        bool _strict = false;
        bool _help_flag = false;

    public:
        enum class arg_type { string_t, bool_t, none_t };

        inline _matcher() = default;
        inline _matcher(int argc, const char **argv, int main_argc, bool space_assignment, bool strict);

        inline void check(bool dec_main_argc);
        inline void check_named();
        inline void check_positional();

        inline std::pair<std::string, arg_type> get_and_mark_as_queried(const identifier &id);
        inline void parse(int argc, const char **argv);
        inline std::vector<std::string> to_vector_string(int n_strings, const char **strings);
        inline std::tuple<std::vector<std::string>, std::vector<std::string>>
                separate_named_positional(const std::vector<std::string> &raw);
        inline std::vector<std::pair<std::string, bool>> split_equations(const std::vector<std::string> &named);
        inline std::vector<std::pair<std::string, optional<std::string>>>
                assign_named_values(const std::vector<std::pair<std::string, bool>> &named);
        inline const std::string& get_executable() { return _executable; }
        inline size_t pos_args() { return _positional.size(); }
        inline bool deferred_assert(const identifier &id, bool pass, const std::string &msg);
    };


    class _help_logger { // Gathers function argument help info here
    public:
        struct log_elem {
            std::string descr;
            std::string type;
            std::string def;
            bool optional;
        };

    private:
        std::vector<std::pair<identifier, log_elem>> _params;

        inline std::string _make_printable(const identifier &id, const log_elem &elem, bool verbose);
        inline void _add_to_help(std::string &usage, std::string &options,
                                 const identifier &id, const log_elem &elem, size_t margin);
    public:
        inline void print_help();
        inline void log(const identifier &name, const log_elem &elem);
    };

    template <typename T_VOID = void>
    struct _storage {
        static _matcher matcher;
        static _help_logger help_logger;
    };

    template <typename T_VOID>
    _matcher _storage<T_VOID>::matcher;

    template <typename T_VOID>
    _help_logger _storage<T_VOID>::help_logger;

    using _ = _storage<void>;

    class arg {
        identifier _id; // No identifier implies vector positional arguments
        std::string _descr;

        optional<long long> _int_value;
        optional<long double> _float_value;
        optional<std::string> _string_value;

        template <typename T>
        optional<T> _get() { T::unimplemented_function; } // no default function

        template <typename T, typename std::enable_if<std::is_integral<T>::value && ! std::is_same<T, bool>::value>::type* = nullptr>
        optional<T> _get_with_precision();
        template <typename T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
        optional<T> _get_with_precision();
        template <typename T, typename std::enable_if<std::is_same<T, bool>::value || std::is_same<T, std::string>::value, bool>::type* = nullptr>
        optional<T> _get_with_precision() { return _get<T>(); }

        template <typename T> optional<T> _convert_optional(bool dec_main_argc=true);
        template <typename T> T _convert(bool dec_main_argc=true);
        inline void _log(const std::string &type, bool optional);

        template <typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
        inline void init_default(T value) { _int_value = value; }
        template <typename T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
        inline void init_default(T value) { _float_value = value; }
        inline void init_default(const std::string &value) { _string_value = value; }
        inline void init_default(std::nullptr_t) {}

        inline arg() = default;

        struct convertible {
            optional<int> _int_value;
            optional<const char *> _char_value;

            convertible(int value): _int_value(value) {}
            convertible(const char *value): _char_value(value) {}
        };

    public:
        inline arg(std::initializer_list<convertible> init) {
            _instant_assert(init.size() == 2, "exactly two arguments must be given as initializer list arguments");
            optional<int> int_value;
            std::vector<const char *> char_values;
            for(const convertible &val: init) {
                if(val._int_value.has_value())
                    int_value = val._int_value.value();
                else
                    char_values.push_back(val._char_value.value());
            }
            _instant_assert(char_values.size() >= 1, "both initializer list arguments can't be ints");

            if(char_values.size() == 2)
                _id = identifier(char_values[0], char_values[1]);
            if(char_values.size() == 1)
                _id = identifier(char_values[0], int_value.value());
        }

        template<typename T=std::nullptr_t>
        inline arg(identifier _id, const char *_descr = "", T value=T()):
                _id(std::move(_id)), _descr(_descr) { init_default(value); }

        inline static arg vector(std::string _descr = "");

        template <typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
        inline operator optional<T>() { _log("INTEGER", true); return _convert_optional<T>(); }
        template <typename T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
        inline operator optional<T>() { _log("REAL", true); return _convert_optional<T>(); }
        inline operator optional<std::string>() { _log("STRING", true); return _convert_optional<std::string>(); }

        template <typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
        inline operator T() { _log("INTEGER", false); return _convert<T>(); }
        template <typename T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
        inline operator T() { _log("REAL", false); return _convert<T>(); }
        inline operator std::string() { _log("STRING", false); return _convert<std::string>(); }
        inline operator bool();

        template <typename T>
        inline operator std::vector<T>();
    };

    void _instant_assert(bool pass, const std::string &msg, bool programmer_side) {
        if (pass)
            return;

        if (!msg.empty()) {
            std::cerr << "Error";
            if(programmer_side)
                std::cerr << " (programmer side)";
            std::cerr << ": " << msg << std::endl;
        }
        exit(_failure_code);
    }

    int count_hyphens(const std::string &s) {
        int hyphens;
        for(hyphens = 0; hyphens < (int) s.size() && s[hyphens] == '-'; ++hyphens)
            ;
        return hyphens;
    }


    std::string identifier::prepend_hyphens(const std::string &name) {
        if(name.size() == 1)
            return "-" + name;
        if(name.size() >= 2)
            return "--" + name;
        return name;
    }

    void identifier::_check_name(const std::string &name) {
        _instant_assert(count_hyphens(name) == 0, "argument " + name +
        " has hyphens prefixed in declaration");
        _instant_assert(name.size() >= 1, "name must contain at least one character");
        _instant_assert(name.size() >= 2 || !isdigit(name[0]), "single character name must not be a digit (" + name + ")");
    }

    identifier::identifier(const char *name_c) {
        std::string name = name_c;
        _check_name(name);

        if(name.size() == 1) _short_name = name;
        else _long_name = name;

        _help = _long_name.has_value() ? ("--" + name) : ("-" + name);
        _longer = _help;
    }

    identifier::identifier(const char *name1_c, const char *name2_c) {
        std::string name1 = name1_c, name2 = name2_c;
        _check_name(name1);
        _check_name(name2);

        if(name2.size() < name1.size())
            std::swap(name1, name2);
        _instant_assert(name1.size() == 1, "one of the two names given must be a shorthand (single character)");
        _instant_assert(name2.size() >= 2, "one of the two names given must be longer than one character");

        _short_name = name1;
        _long_name = name2;
        _help = "-" + name1 + "|--" + name2;
        _longer = "--" + name2;
    }

    bool identifier::operator<(const identifier &other) const {
        std::string name = _long_name.value_or(_short_name.value_or(""));
        std::string other_name = other._long_name.value_or(other._short_name.value_or(""));

        std::transform(name.begin(), name.end(), name.begin(), [](char c){ return (char) tolower(c); });
        std::transform(other_name.begin(), other_name.end(), other_name.begin(), [](char c){ return (char) tolower(c); });

        if(name != other_name)
            return name < other_name;
        return _pos.value_or(1000000) < other._pos.value_or(1000000);
    }

    bool identifier::overlaps(const identifier &other) const {
        if(_long_name.has_value() && other._long_name.has_value())
            if(_long_name.value() == other._long_name.value())
                return true;
        if(_short_name.has_value() && other._short_name.has_value())
            if(_short_name.value() == other._short_name.value())
                return true;
        if(_pos.has_value() && other._pos.has_value())
            if(_pos.value() == other._pos.value())
                return true;
        return false;
    }

    bool identifier::contains(const std::string &name) const {
        if(_short_name.has_value() && name == _short_name.value()) return true;
        if(_long_name.has_value() && name == _long_name.value()) return true;
        return false;
    }

    bool identifier::contains(int pos) const {
        if(_pos.has_value() && pos == _pos.value()) return true;
        return false;
    }


    template<typename ORDER, typename VALUE>
    void _first<ORDER, VALUE>::set(const ORDER &__order, const VALUE &__value) {
        if(_empty || __order < _order) {
            _order = __order;
            _value = __value;
            _empty = false;
        }
    }

    template<typename ORDER, typename VALUE>
    const VALUE & _first<ORDER, VALUE>::get() const {
        return _value;
    }


    _matcher::_matcher(int argc, const char **argv, int main_argc, bool space_assignment, bool strict) {
        _main_argc = main_argc;
        _space_assignment = space_assignment;
        _strict = strict;

        parse(argc, argv);
        _help_flag = get_and_mark_as_queried(identifier({"h", "help"})).second != arg_type::none_t;
        check(false);
    }

    void _matcher::check(bool dec_main_argc) {
        _main_argc -= dec_main_argc;
        if(! _strict || _main_argc > 0) return;

        if(_help_flag) {
            _::help_logger.print_help();
            exit(0);
        }

        check_named();
        check_positional();

        if(! _deferred_error.empty()) {
            std::cerr << "Error: " << _deferred_error.get() << std::endl;
            exit(_failure_code);
        }
    }

    void _matcher::check_named() {
        int invalid_count = 0;
        std::string invalid;
        for(const auto &it: _named) {
            for(const auto &jt: _queried)
                if(jt.contains(it.first))
                    goto VALID;

            ++invalid_count;
            invalid += " " + identifier::prepend_hyphens(it.first);
            VALID:;
        }
        deferred_assert(identifier(), invalid.empty(),
                        std::string("invalid argument") + (invalid_count > 1 ? "s" : "") + invalid);
    }

    void _matcher::check_positional() {
        int invalid_count = 0;
        std::string invalid;
        for(size_t i = 0; i < _positional.size(); ++i) {
            for(const auto &it: _queried)
                if(it.contains((int) i))
                    goto VALID;

            ++invalid_count;
            invalid += " " + std::to_string(i);
            VALID:;
        }
        deferred_assert(identifier(), invalid.empty(),
                        std::string("invalid positional argument") + (invalid_count > 1 ? "s" : "") + invalid);
    }

    std::pair<std::string, _matcher::arg_type> _matcher::get_and_mark_as_queried(const identifier &id) {
        if(_space_assignment)
            _instant_assert(! id.get_pos().has_value(), "positional argument used with space assignement enabled: (disable space assignement by calling FIRE_NO_SPACE_ASSIGNMENT(...) instead of FIRE(...))");

        for(const auto& it: _queried)
            _instant_assert(! it.overlaps(id), "double query for argument " + id.longer());

        if (_strict)
            _queried.push_back(id);

        for(auto it = _named.begin(); it != _named.end(); ++it) {
            if (id.contains(it->first)) {
                optional<std::string> result = it->second;
                if (result.has_value())
                    return {result.value(), arg_type::string_t};
                return {"", arg_type::bool_t};
            }
        }

        if(id.get_pos().has_value()) {
            size_t pos = id.get_pos().value();
            if(pos >= _positional.size())
                return {"", arg_type::none_t};

            return {_positional[pos], arg_type::string_t};
        }

        return {"", arg_type::none_t};
    }

    void _matcher::parse(int argc, const char **argv) {
        _executable = argv[0];
        std::vector<std::string> raw = to_vector_string(argc - 1, argv + 1);
        std::vector<std::string> named;
        tie(named, _positional) = separate_named_positional(raw);
        std::vector<std::pair<std::string, bool>> split = split_equations(named);
        _named = assign_named_values(split);

        for(size_t i = 0; i < _named.size(); ++i)
            for(size_t j = 0; j < i; ++j)
                deferred_assert(identifier(), _named[i].first != _named[j].first,
                                "multiple occurrences of argument " + identifier::prepend_hyphens(_named[i].first));

        if(_space_assignment)
            deferred_assert(identifier(), _positional.empty(), "positional arguments given, but not accepted");
    }

    std::vector<std::string> _matcher::to_vector_string(int n_strings, const char **strings) {
        std::vector<std::string> raw(n_strings);
        for(int i = 0; i < n_strings; ++i)
            raw[i] = strings[i];
        return raw;
    }

    std::tuple<std::vector<std::string>, std::vector<std::string>>
            _matcher::separate_named_positional(const std::vector<std::string> &raw) {
        std::vector<std::string> named, positional;

        bool to_named = false;
        for(const std::string &s: raw) {
            int hyphens = count_hyphens(s);
            int name_size = (int) s.size() - hyphens;
            deferred_assert(identifier(), hyphens <= 2, "too many hyphens: " + s);
            if(hyphens == 2 || (hyphens == 1 && name_size >= 1 && !isdigit(s[1]))) {
                named.push_back(s);
                to_named = hyphens >= 2 || name_size == 1; // Not "-abc" == "-a -b -c"
                to_named &= (s.find('=') == std::string::npos); // No equation signs
                continue;
            }
            if(_space_assignment && to_named) {
                named.push_back(s);
                to_named = false;
                continue;
            }
            positional.push_back(s);
        }

        return std::tuple<std::vector<std::string>, std::vector<std::string>>(named, positional);
    }

    std::vector<std::pair<std::string, bool>> _matcher::split_equations(const std::vector<std::string> &named) {
        std::vector<std::pair<std::string, bool>> split; // std::string: parsed string, bool: is certainly value
        for(const std::string &hyphened_name: named) {
            int hyphens = count_hyphens(hyphened_name);
            size_t eq = hyphened_name.find('=');
            if(eq == std::string::npos) {
                split.emplace_back(hyphened_name, false);
                continue;
            }
            int name_size = (int) eq - hyphens;

            if(!deferred_assert(identifier(), name_size == 1 || hyphens >= 2,
                                "expanding single-hyphen arguments can't have value (" + hyphened_name + ")")) continue;

            split.emplace_back(hyphened_name.substr(0, eq), false);
            split.emplace_back(hyphened_name.substr(eq + 1), true);
        }
        return split;
    }

    std::vector<std::pair<std::string, optional<std::string>>>
            _matcher::assign_named_values(const std::vector<std::pair<std::string, bool>> &named) {
        std::vector<std::pair<std::string, optional<std::string>>> args;

        for(const std::pair<std::string, bool> &p: named) {
            const std::string &hyphened_name = p.first;
            bool certainly_value = p.second;

            int hyphens = count_hyphens(hyphened_name);
            std::string name = hyphened_name.substr(hyphens);
            if(certainly_value) {
                args.back().second = hyphened_name;
            } else if(hyphens == 2) {
                deferred_assert(identifier(), name.size() >= 2,
                                "single character parameter " + hyphened_name + " must have exactly one hyphen");
                args.emplace_back(name, optional<std::string>());
            } else if(hyphens == 1) {
                if(isdigit(name[0]))
                    args.back().second = hyphened_name;
                else
                    for (char c: name)
                        args.emplace_back(std::string(1, c), optional<std::string>());
            } else if(hyphens == 0)
                args.back().second = name;
        }
        return args;
    }

    bool _matcher::deferred_assert(const identifier &id, bool pass, const std::string &msg) {
        if(! _strict) {
            _instant_assert(pass, msg, false);
            return pass;
        }
        if(! pass)
            _deferred_error.set(id, msg);
        return pass;
    }

    std::string _help_logger::_make_printable(const identifier &id, const log_elem &elem, bool verbose) {
        std::string printable;
        if(elem.optional || elem.type == "") printable += "[";
        printable += verbose ? id.help() : id.longer();
        if(elem.type != "" && ! (! verbose && id.get_pos().has_value())) {
            printable += "=<";
            printable += elem.type;
            printable += ">";
        }
        if(elem.optional || elem.type == "") printable += "]";
        return printable;
    }

    void _help_logger::_add_to_help(std::string &usage, std::string &options,
                                    const identifier &id, const log_elem &elem, size_t margin) {
        usage += " ";
        usage += _make_printable(id, elem, false);

        std::string printable = _make_printable(id, elem, true);
        options += "      " + printable + std::string(2 + margin - printable.size(), ' ') + elem.descr;
        if(! elem.def.empty())
            options += " [default: " + elem.def + "]";
        options += "\n";
    }

    void _help_logger::print_help() {
        using id2elem = std::pair<identifier, log_elem>;

        std::string usage = "    Usage:\n      " + _::matcher.get_executable();
        std::string options = "    Options:\n";

        std::vector<std::pair<identifier, log_elem>> printed(_params);
        std::sort(printed.begin(), printed.end(), [](const id2elem &a, const id2elem &b) {
            if(a.second.optional != b.second.optional)
                return a.second.optional < b.second.optional;
            return a.first < b.first;
        });

        size_t margin = 0;
        for(const auto& it: printed)
            margin = std::max(margin, _make_printable(it.first, it.second, true).size());

        for(const auto& it: printed)
            _add_to_help(usage, options, it.first, it.second, margin);

        std::cerr << std::endl << usage << std::endl << std::endl << std::endl << options << std::endl;
    }

    void _help_logger::log(const identifier &name, const log_elem &_elem) {
        log_elem elem = _elem;
        elem.optional |= ! elem.def.empty();
        _params.emplace_back(name, elem);
    }

    template <>
    inline optional<long long> arg::_get<long long>() {
        auto elem = _::matcher.get_and_mark_as_queried(_id);
        _::matcher.deferred_assert(_id, elem.second != _matcher::arg_type::bool_t,
                                   "argument " + _id.help() + " must have value");
        if(elem.second == _matcher::arg_type::string_t) {
            size_t last = 0;
            bool is_int = true;
            long long converted = 0;
            try {
                converted = std::stoll(elem.first, &last);
            } catch(std::out_of_range &) {
                _::matcher.deferred_assert(_id, false, "value " + elem.first + " out of range");
            } catch(std::invalid_argument &) {
                is_int = false;
            }

            _::matcher.deferred_assert(_id, is_int && last == elem.first.size(), // last != elem.first.size() indicates floating point
                                       "value " + elem.first + " is not an integer");

            return converted;
        }

        return _int_value;
    }

    template <>
    inline optional<long double> arg::_get<long double>() {
        auto elem = _::matcher.get_and_mark_as_queried(_id);
        _::matcher.deferred_assert(_id, elem.second != _matcher::arg_type::bool_t,
                                   "argument " + _id.help() + " must have value");
        if(elem.second == _matcher::arg_type::string_t) {
            try {
                return std::stold(elem.first);
            } catch(std::out_of_range &) {
                _::matcher.deferred_assert(_id, false, "value " + elem.first + " out of range");
            } catch(std::invalid_argument &) {
                _::matcher.deferred_assert(_id, false, "value " + elem.first + " is not a real number");
            }
        }

        if(_float_value.has_value()) return _float_value;
        if(_int_value.has_value()) return (long double) _int_value.value();
        return {};
    }

    template <>
    inline optional<std::string> arg::_get<std::string>() {
        auto elem = _::matcher.get_and_mark_as_queried(_id);
        _::matcher.deferred_assert(_id, elem.second != _matcher::arg_type::bool_t,
                                   "argument " + _id.help() + " must have value");

        if(elem.second == _matcher::arg_type::string_t)
            return elem.first;
        return _string_value;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value && ! std::is_same<T, bool>::value>::type*>
    optional<T> arg::_get_with_precision() {
        optional<long long> opt_value = _get<long long>();
        if(! opt_value.has_value())
            return optional<T>();
        long long value = opt_value.value();

        bool is_signed = std::numeric_limits<T>::is_signed;
        T min = std::numeric_limits<T>::lowest();
        T max = std::numeric_limits<T>::max();

        _::matcher.deferred_assert(_id, is_signed || value >= 0,
                                   "argument " + _id.help() + " must be positive");
        _::matcher.deferred_assert(_id, min <= value && value <= max,
                                   "value " + std::to_string(value) + " out of range");

        return (T) value;
    }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value>::type*>
    optional<T> arg::_get_with_precision() {
        optional<long double> opt_value = _get<long double>();
        if(! opt_value.has_value())
            return optional<T>();
        long double value = opt_value.value();

        T min = std::numeric_limits<T>::lowest();
        T max = std::numeric_limits<T>::max();

        _::matcher.deferred_assert(_id, min <= value && value <= max,
                                   "value " + std::to_string(value) + " out of range");

        return (T) value;
    }

    template <typename T>
    optional<T> arg::_convert_optional(bool dec_main_argc) {
        _instant_assert(! (_int_value.has_value() || _float_value.has_value() || _string_value.has_value()),
                        "optional argument has default value");
        optional<T> val = _get_with_precision<T>();
        _::matcher.check(dec_main_argc);
        return val;
    }

    template <typename T>
    T arg::_convert(bool dec_main_argc) {
        optional<T> val = _get_with_precision<T>();
        _::matcher.deferred_assert(_id, val.has_value(),
                                   "required argument " + _id.longer() + " not provided");
        _::matcher.check(dec_main_argc);
        return val.value_or(T());
    }

    void arg::_log(const std::string &type, bool optional) {
        std::string def;
        if(_int_value.has_value()) def = std::to_string(_int_value.value());
        if(_float_value.has_value()) def = std::to_string(_float_value.value());
        if(_string_value.has_value()) def = _string_value.value();

        _::help_logger.log(_id, {_descr, type, def, optional});
    }

    arg arg::vector(std::string descr) {
        arg a;
        a._descr = std::move(descr);
        return a;
    }

    arg::operator bool() {
        _instant_assert(!_int_value.has_value() && !_float_value.has_value() && !_string_value.has_value(),
                _id.longer() + " flag parameter must not have default value");

        _log("", true); // User sees this as flag, not boolean option
        auto elem = _::matcher.get_and_mark_as_queried(_id);
        _::matcher.deferred_assert(_id, elem.second != _matcher::arg_type::string_t,
                                   "flag " + _id.help() + " must not have value");
        _::matcher.check(true);
        return elem.second == _matcher::arg_type::bool_t;
    }

    template <typename T>
    arg::operator std::vector<T>() {
        std::vector<T> ret;
        for(size_t i = 0; i < _::matcher.pos_args(); ++i)
            ret.push_back(arg((int) i)._convert<T>(false));
        _log("", true);
        _::matcher.check(true);
        return ret;
    }
}



template<typename F>
void init_and_run(int argc, const char ** argv, F main_func, bool space_assignment) {
    int main_argc = (int) fire::_get_argument_count(main_func);
    bool strict = true;
    fire::_::help_logger = fire::_help_logger();
    fire::_::matcher = fire::_matcher(argc, argv, main_argc, space_assignment, strict);
}

#define FIRE(fired_main) \
int main(int argc, const char ** argv) {\
    bool space_assignment = true;\
    init_and_run(argc, argv, fired_main, space_assignment);\
    return fired_main();\
}

#define FIRE_NO_SPACE_ASSIGNMENT(fired_main) \
int main(int argc, const char ** argv) {\
    bool space_assignment = false;\
    init_and_run(argc, argv, fired_main, space_assignment);\
    return fired_main();\
}

#endif
