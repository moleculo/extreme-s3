#ifndef ERRORS_H
#define ERRORS_H

#include "common.h"
#include <sstream>

namespace es3 {
	class die_t {};
	extern ES3LIB_PUBLIC const die_t die;

	class result_code_t
	{
	public:
		enum code_e {
			sOk = 200,
			sAccepted = 202,
			sCreated = 201,
			sNotFound = 404,
			sConflict = 409,
			sWarnings = 300,
			sError = 500,
		};

		result_code_t() : code_(sOk), desc_()
		{
		}

		result_code_t(code_e code, const std::string &desc="None") :
			code_(code), desc_(desc)
		{

		}

		virtual ~result_code_t(){}

		code_e code() const { return code_; }
		const std::string& desc() const { return desc_; }
		bool ok() const {return code_>=sOk && code_<=sCreated;}
	private:
		code_e code_;
		std::string desc_;
	};
	extern ES3LIB_PUBLIC const result_code_t sok;

	class es3_exception : public virtual boost::exception,
			public std::exception
	{
		const result_code_t code_;
		std::string what_;
	public:
		ES3LIB_PUBLIC es3_exception(const result_code_t &code);
		virtual ~es3_exception() throw() {}

		const result_code_t & err() const
		{
			return code_;
		}

		virtual const char* what() const throw()
		{
			return what_.c_str();
		}
	};

	/**
	  Usage - err(sOk) << "This is a description"
	  */
	struct err : public std::stringstream
	{
		err(result_code_t::code_e code) : code_(code) {}

		~err()
		{
			//Yes, we're throwing from a destructor, but that's OK since
			//if there's an exception already started then we have other
			//big problems.
			if (code_!=result_code_t::sOk)
			{
				boost::throw_exception(es3_exception(
										   result_code_t(code_, str())));
			}
		}

	private:
		result_code_t::code_e code_;
	};

	inline void operator | (const result_code_t &code, const die_t &)
	{
		if (!code.ok())
			boost::throw_exception(es3_exception(code));
	}

#define TRYIT(expr) try{ expr; } catch(const es3_exception &ex) { \
		return ex.err(); \
	}

}; //namespace es3

#endif //ERRORS_H
