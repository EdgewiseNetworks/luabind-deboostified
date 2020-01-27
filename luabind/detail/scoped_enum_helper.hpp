// Copyright (c) 2020 Edgewise Networks, Inc. All rights reserved.

// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
// ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
// SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
// ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
// OR OTHER DEALINGS IN THE SOFTWARE.


#ifndef LUABIND_SCOPED_ENUM_HELPER_HPP_INCLUDED
#define LUABIND_SCOPED_ENUM_HELPER_HPP_INCLUDED

#include <limits>
#include <type_traits>

#include <luabind/config.hpp>
#include <luabind/detail/class_rep.hpp>

/**
 * @file scoped_enum_helper.hpp
 *
 * @description helpers for converting C++11 scoped enums back and forth to lua's lua_Number type and luabind's enum_ static constant values
 *
 * @note LuaNumberType is a template parameter because in parts of luabind, they use int for static class constants, rather than luajit's #define of double
 */

namespace luabind {

	template <typename T, T v>
	using scoped_enum_value = std::integral_constant<T, v>;

	namespace detail {

		enum traits_test_enum { };

		/** metafunction to discriminate between classic enums and C++11 scoped enums */
		template <typename T>
		using is_scoped_enum = std::integral_constant
		<
			bool,
			(std::is_enum<T>::value && !std::is_convertible<T, int>::value)
		>;

		/**
		 * traits for casting between integral and floating types.
		 *
		 * @note this isn't great, but it's true for current implementations of float, double, and long double
		 */
		template <typename Int, typename Float> struct is_float_castable_ : std::integral_constant<bool, (sizeof(Int) < sizeof (Float))> { };

		template <typename T, typename LuaNumberType, typename = void> struct is_float_castable;
		template <typename T, typename LuaNumberType>
		struct is_float_castable<T, LuaNumberType, std::enable_if_t<std::is_floating_point<LuaNumberType>::value>>
			: is_float_castable_<T, LuaNumberType>
		{ };
		template <typename T, typename LuaNumberType>
		struct is_float_castable<T, LuaNumberType, std::enable_if_t<!std::is_floating_point<LuaNumberType>::value>>
			: std::false_type
		{ };

		/** determine whether all of Src fits within Dst and Src s == static_cast<Src>(static_cast<Dst>(s)) */
		template <typename Src, typename Dst>
		struct is_integer_castable_
			: std::integral_constant<bool, ((sizeof(Src) < sizeof (Dst)) ||
			                                ((sizeof(Src) == sizeof(Dst)) &&
			                                 (std::is_signed<Src>::value == std::is_signed<Dst>::value)))>
		{ };

		template <typename T, typename LuaNumberType, typename = void> struct is_integer_castable;
		template <typename T, typename LuaNumberType>
		struct is_integer_castable<T, LuaNumberType, std::enable_if_t<std::is_integral<LuaNumberType>::value && !is_scoped_enum<T>::value>>
			: is_integer_castable_<T, LuaNumberType>
		{ };
		template <typename T, typename LuaNumberType>
		struct is_integer_castable<T, LuaNumberType, std::enable_if_t<std::is_integral<LuaNumberType>::value && is_scoped_enum<T>::value>>
			: is_integer_castable_<std::underlying_type_t<T>, LuaNumberType>
		{ };
		template <typename T, typename LuaNumberType>
		struct is_integer_castable<T, LuaNumberType, std::enable_if_t<!std::is_integral<LuaNumberType>::value>>
			: std::false_type
		{ };

		/**
		 * metafunction to make sure an enum type meets the (clearly pre-C++11 enum) luabind storage reqs.
		 *
		 * @note
		 *    remember, the C++ standard says uints > int_max can be casted in any implementation-defined way,
		 *    so sound the alarm if we try to do any shenanigans here with implicit casts.  It's far better to
		 *    get a static_assert than have hard-to-detect undefined behavior...
		 */
		template <typename T, typename LuaNumberType>
		using is_ok_enum = std::integral_constant
		<
			bool,
			(is_integer_castable<T, LuaNumberType>::value || is_float_castable<T, LuaNumberType>::value)
		>;

		/**
		 * internal helper class for doing casts between enum and luabind's underlying int storage container.
		 */
		template <typename T, typename LuaNumberType, typename = void> struct unchecked_enum_helper;
		template <typename T, typename LuaNumberType>
		struct unchecked_enum_helper<T, LuaNumberType, std::enable_if_t<!is_scoped_enum<T>::value>>
		{
			static constexpr LuaNumberType CastToLuaNumber(T v) noexcept
			{
				return static_cast<LuaNumberType>(v);
			}
			static constexpr T CastFromLuaNumber(LuaNumberType v) noexcept
			{
				return static_cast<T>(v);
			}
		};
		template <typename T, typename LuaNumberType>
		struct unchecked_enum_helper<T, LuaNumberType, std::enable_if_t<is_scoped_enum<T>::value>>
		{
			static constexpr int CastToLuaNumber(T v) noexcept
			{
				return static_cast<LuaNumberType>(static_cast<std::underlying_type_t<T>>(v));
			}
			static constexpr T CastFromLuaNumber(LuaNumberType v) noexcept
			{
				return static_cast<T>(static_cast<std::underlying_type_t<T>>(v));
			}
		};

		/**
		 * helper class for doing casts between enum and luabind's underlying int storage container.
		 */
		template <typename T, typename LuaNumberType, typename = void> struct enum_helper;
		template <typename T, typename LuaNumberType>
		struct enum_helper<T, LuaNumberType, std::enable_if_t<!is_scoped_enum<T>::value>>
			: unchecked_enum_helper<T, LuaNumberType>
		{
			using super = unchecked_enum_helper<T, LuaNumberType>;
			using super::CastToLuaNumber;
			using super::CastFromLuaNumber;
		};
		template <typename T, typename LuaNumberType>
		struct enum_helper<T, LuaNumberType, std::enable_if_t<(is_scoped_enum<T>::value && is_ok_enum<T, LuaNumberType>::value)>>
			: unchecked_enum_helper<T, LuaNumberType>
		{
			using super = unchecked_enum_helper<T, LuaNumberType>;
			using super::CastToLuaNumber;
			using super::CastFromLuaNumber;
		};
		template <typename T, typename LuaNumberType>
		struct enum_helper<T, LuaNumberType, std::enable_if_t<(is_scoped_enum<T>::value && !is_ok_enum<T, LuaNumberType>::value)>>
		{
			static_assert(is_ok_enum<T, LuaNumberType>::value, "this scoped enum type is not compatible with luabind");
		};

		/**
		 * traits for casting between integral and floating types for a specific integer value.
		 *
		 * @note these are overly conservative, but they're bigger than most enum values we're ever likely to see
		 */
		template <typename Int, Int v, typename Float, typename = void> struct is_constexpr_float_castable_;
		template <typename Int, Int v> struct is_constexpr_float_castable_<Int, v, float>       : std::integral_constant<bool, (v <= std::numeric_limits<uint16_t>::max())> { };
		template <typename Int, Int v> struct is_constexpr_float_castable_<Int, v, double>      : std::integral_constant<bool, (v <= std::numeric_limits<uint32_t>::max())> { };
		template <typename Int, Int v> struct is_constexpr_float_castable_<Int, v, long double> : std::integral_constant<bool, (v <= std::numeric_limits<uint64_t>::max())> { };

		template <typename T, T v, typename LuaNumberType, typename = void> struct is_constexpr_float_castable;
		template <typename T, T v, typename LuaNumberType>
		struct is_constexpr_float_castable<T, v, LuaNumberType, std::enable_if_t<std::is_floating_point<LuaNumberType>::value>>
			: is_constexpr_float_castable_<std::underlying_type_t<T>, static_cast<std::underlying_type_t<T>>(v), LuaNumberType>
		{ };
		template <typename T, T v, typename LuaNumberType>
		struct is_constexpr_float_castable<T, v, LuaNumberType, std::enable_if_t<!std::is_floating_point<LuaNumberType>::value>>
			: std::false_type
		{ };

		/** determine whether v fits within Dst's bit width */
		template <typename Src, Src v, typename Dst> using is_constexpr_integer_castable_ = std::integral_constant<bool, (v <= std::numeric_limits<Dst>::max())>;

		template <typename T, T v, typename LuaNumberType, typename = void> struct is_constexpr_integer_castable;
		template <typename T, T v, typename LuaNumberType>
		struct is_constexpr_integer_castable<T, v, LuaNumberType, std::enable_if_t<std::is_integral<LuaNumberType>::value && !is_scoped_enum<T>::value>>
			: is_constexpr_integer_castable_<T, v, LuaNumberType>
		{ };
		template <typename T, T v, typename LuaNumberType>
		struct is_constexpr_integer_castable<T, v, LuaNumberType, std::enable_if_t<std::is_integral<LuaNumberType>::value && is_scoped_enum<T>::value>>
			: is_constexpr_integer_castable_<std::underlying_type_t<T>, static_cast<std::underlying_type_t<T>>(v), LuaNumberType>
		{ };
		template <typename T, T v, typename LuaNumberType>
		struct is_constexpr_integer_castable<T, v, LuaNumberType, std::enable_if_t<!std::is_integral<LuaNumberType>::value>>
			: std::false_type
		{ };

		/**
		 * metafunction to make sure an enum type meets the (clearly pre-C++11 enum) luabind storage reqs.
		 *
		 * @note
		 *    remember, the C++ standard says uints > int_max can be casted in any implementation-defined way,
		 *    so sound the alarm if we try to do any shenanigans here with implicit casts.  It's far better to
		 *    get a static_assert than have hard-to-detect undefined behavior...
		 */
		template <typename T, T v, typename LuaNumberType>
		using is_constexpr_ok_enum = std::integral_constant
		<
			bool,
			(is_ok_enum<T, LuaNumberType>::value ||
			 is_constexpr_integer_castable<T, v, LuaNumberType>::value ||
			 is_constexpr_float_castable<T, v, LuaNumberType>::value)
		>;

			  /* old way: static_cast<std::underlying_type_t<T>>(v) <= static_cast<unsigned>(std::numeric_limits<int>::max()))) */

		/**
		 * helper class for doing casts between enum and luabind's underlying int storage container.
		 */
		template <typename T, T v, typename LuaNumberType, typename = void> struct constexpr_enum_helper;
		template <typename T, T v, typename LuaNumberType>
		struct constexpr_enum_helper<T, v, LuaNumberType, std::enable_if_t<is_constexpr_ok_enum<T, v, LuaNumberType>::value>>
		{
			static constexpr LuaNumberType Value = unchecked_enum_helper<T, LuaNumberType>::CastToLuaNumber(v);
		};
		template <typename T, T v, typename LuaNumberType>
		struct constexpr_enum_helper<T, v, LuaNumberType, std::enable_if_t<!is_constexpr_ok_enum<T, v, LuaNumberType>::value>>
		{
			static_assert(is_constexpr_ok_enum<T, v, LuaNumberType>::value, "this scoped enumeration value is not compatible with luabind");
		};

		namespace tests
		{
			enum class bar{ OLD_FASIONED };
			enum bender{ OLD_FORTRAN };
			static_assert(!is_scoped_enum<bender>::value, "bender is a classic robot, er, enum, so that's a problem...");
			static_assert(is_scoped_enum<bar>::value, "bar is a scoped enum, so that's a problem...");

			template <typename T>
			struct bear
			{
				enum class porridge : T
				{
					SUPERFLUID_HELIUM = 0,
					GOLDILOCKS = std::numeric_limits<int>::max(),
					COOKED_IN_A_TOKAMAK = std::numeric_limits<T>::max()
				};
			};
			static_assert(std::numeric_limits<int>::max() == static_cast<int>(static_cast<unsigned>(bear<unsigned>::porridge::GOLDILOCKS)), "feh");
			static_assert(is_ok_enum<bear<int>::porridge, int>::value, "uh, bear<int> must work by definition");
			static_assert(!is_ok_enum<bear<unsigned>::porridge, int>::value, "uh, bear<unsigned> can't generally be casted to int");
			static_assert(!is_ok_enum<bear<int64_t>::porridge, int>::value, "uh, bear<int64_t> is too big to represent in an int");
			static_assert(!is_ok_enum<bear<uint64_t>::porridge, int>::value, "uh, bear<uint64_t> is too big to represent in an int");
			static_assert(is_constexpr_ok_enum<bear<unsigned>::porridge, bear<unsigned>::porridge::SUPERFLUID_HELIUM, int>::value, "uh, bear<unsigned>::porridge::SUPERFLUID_HELIUM is safely static_cast-able to int");
			static_assert(is_constexpr_ok_enum<bear<unsigned>::porridge, bear<unsigned>::porridge::GOLDILOCKS, int>::value, "uh, bear<unsigned>::porridge::GOLDILOCKS is safely static_cast-able to int");
			static_assert(!is_constexpr_ok_enum<bear<unsigned>::porridge, bear<unsigned>::porridge::COOKED_IN_A_TOKAMAK, int>::value, "uh, bear<unsigned>::porridge::COOKED_IN_A_TOKAMAK is not safely static_cast-able to int");
			static_assert(is_constexpr_ok_enum<bear<int64_t>::porridge, bear<int64_t>::porridge::SUPERFLUID_HELIUM, int>::value, "uh, bear<int64_t>::porridge::SUPERFLUID_HELIUM is safely static_cast-able to int");
			static_assert(is_constexpr_ok_enum<bear<int64_t>::porridge, bear<int64_t>::porridge::GOLDILOCKS, int>::value, "uh, bear<int64_t>::porridge::GOLDILOCKS is safely static_cast-able to int");
			static_assert(!is_constexpr_ok_enum<bear<int64_t>::porridge, bear<int64_t>::porridge::COOKED_IN_A_TOKAMAK, int>::value, "uh, bear<int64_t>::porridge::COOKED_IN_A_TOKAMAK is not safely static_cast-able to int");
			static_assert(is_constexpr_ok_enum<bear<uint64_t>::porridge, bear<uint64_t>::porridge::SUPERFLUID_HELIUM, int>::value, "uh, bear<uint64_t>::porridge::SUPERFLUID_HELIUM is safely static_cast-able to int");
			static_assert(is_constexpr_ok_enum<bear<uint64_t>::porridge, bear<uint64_t>::porridge::GOLDILOCKS, int>::value, "uh, bear<uint64_t>::porridge::GOLDILOCKS is safely static_cast-able to int");
			static_assert(!is_constexpr_ok_enum<bear<uint64_t>::porridge, bear<uint64_t>::porridge::COOKED_IN_A_TOKAMAK, int>::value, "uh, bear<uint64_t>::porridge::COOKED_IN_A_TOKAMAK is not safely static_cast-able to int");

			static_assert(is_ok_enum<bear<int>::porridge, double>::value, "uh, bear<int> must work by definition");
			static_assert(!is_ok_enum<bear<unsigned>::porridge, int>::value, "uh, bear<unsigned> can't generally be casted to int");
			static_assert(!is_ok_enum<bear<int64_t>::porridge, double>::value, "uh, bear<int64_t> is too big to represent in a double");
			static_assert(!is_ok_enum<bear<uint64_t>::porridge, double>::value, "uh, bear<uint64_t> is too big to represent in a double");
			static_assert(is_constexpr_ok_enum<bear<unsigned>::porridge, bear<unsigned>::porridge::SUPERFLUID_HELIUM, double>::value, "uh, bear<unsigned>::porridge::SUPERFLUID_HELIUM is safely static_cast-able to double");
			static_assert(is_constexpr_ok_enum<bear<unsigned>::porridge, bear<unsigned>::porridge::GOLDILOCKS, double>::value, "uh, bear<unsigned>::porridge::GOLDILOCKS is safely static_cast-able to double");
			static_assert(is_constexpr_ok_enum<bear<unsigned>::porridge, bear<unsigned>::porridge::COOKED_IN_A_TOKAMAK, double>::value, "uh, bear<unsigned>::porridge::COOKED_IN_A_TOKAMAK is safely static_cast-able to double");
			static_assert(is_constexpr_ok_enum<bear<int64_t>::porridge, bear<int64_t>::porridge::SUPERFLUID_HELIUM, double>::value, "uh, bear<int64_t>::porridge::SUPERFLUID_HELIUM is safely static_cast-able to double");
			static_assert(is_constexpr_ok_enum<bear<int64_t>::porridge, bear<int64_t>::porridge::GOLDILOCKS, double>::value, "uh, bear<int64_t>::porridge::GOLDILOCKS is safely static_cast-able to double");
			static_assert(!is_constexpr_ok_enum<bear<int64_t>::porridge, bear<int64_t>::porridge::COOKED_IN_A_TOKAMAK, double>::value, "uh, bear<int64_t>::porridge::COOKED_IN_A_TOKAMAK is not safely static_cast-able to double");
			static_assert(is_constexpr_ok_enum<bear<uint64_t>::porridge, bear<uint64_t>::porridge::SUPERFLUID_HELIUM, double>::value, "uh, bear<uint64_t>::porridge::SUPERFLUID_HELIUM is safely static_cast-able to double");
			static_assert(is_constexpr_ok_enum<bear<uint64_t>::porridge, bear<uint64_t>::porridge::GOLDILOCKS, double>::value, "uh, bear<uint64_t>::porridge::GOLDILOCKS is safely static_cast-able to double");
			static_assert(!is_constexpr_ok_enum<bear<uint64_t>::porridge, bear<uint64_t>::porridge::COOKED_IN_A_TOKAMAK, double>::value, "uh, bear<uint64_t>::porridge::COOKED_IN_A_TOKAMAK is not safely static_cast-able to double");
		}
	}
}

#endif // LUABIND_SCOPED_ENUM_HELPER_HPP_INCLUDED

