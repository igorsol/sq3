#pragma once

//atl
#if defined(_WTL_NO_CSTRING)
#include <atlstr.h>
#else
#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>
#endif

//additional 
#include <sqlite3.h>

// sqlite3wrap
namespace sq3
{
	class exception
	{
	public:
		LPSTR errmsg;
		int code;
		exception(int acode,LPSTR emsg=NULL):code(acode),errmsg(emsg){}
		// выделяет память с помощью SysAllocString и копирует туда строку
		// позволяет инициализировать _bstr_t без лишних копирований, примерно вот так:
		// _bstr_t bStr(BSTR(e),false);
		// где e - объект класса exception
		operator BSTR()const
		{
			size_t len=_mbstrlen(errmsg);
			BSTR r=SysAllocStringLen(NULL,len);	//выделяет память под len+1 символ
#ifdef _DEBUG
			size_t copied=
#endif
			MultiByteToWideChar(CP_UTF8,0,errmsg,-1,r,len+1);
			_ASSERTE(len+1 == copied);
			return r;
		}
	};

	/* Все строки sqlite3 принимает в виде UTF-8 за исключением некоторых спец функций*/
	/* поэтому, для преобразования в UTF-8 создаём спец. макро и функцию*/
#if defined(_UNICODE)
#define MAKEUTF8(x)	\
	CT2A u8##x(x,CP_UTF8);
#else

#if 0
// этот вариант вызывает проблемы, когда sq3.h включен в несколько исходников (функция utilUTF8 определена в нескольких модулях)
	void utilUTF8(LPCTSTR src,LPSTR dst,int u8len)
	{
#if !defined(_UNICODE)
		_bstr_t bstrtmp(src);
		LPCWSTR widetmp=bstrtmp;
#else
		LPCWSTR widetmp=src;
#endif
		WideCharToMultiByte(CP_UTF8,0,widetmp,-1,dst,u8len,NULL,NULL);
	}

#define MAKEUTF8(x)	\
	LPSTR u8##x;	\
	{	\
		int u8len=(lstrlen(x)+1)*4;	\
		u8##x=LPSTR(_alloca(u8len));	\
		utilUTF8(x,u8##x,u8len);	\
	}

#else
// с помощью вспомогательного класса, аналогичного CT2A
class CA2UTF8
{
public:
	CA2UTF8(LPCSTR psz):m_psz(NULL)
	{
		_bstr_t bstrtmp(psz);
		LPCWSTR widetmp=bstrtmp;
		int cbutf=::WideCharToMultiByte(CP_UTF8,0,widetmp,-1,NULL,0,NULL,NULL);
		m_psz=new CHAR[cbutf];
		if(m_psz)
			::WideCharToMultiByte(CP_UTF8,0,widetmp,-1,m_psz,cbutf,NULL,NULL);
	}

	~CA2UTF8() throw()
	{
		if(m_psz)
			delete[]m_psz;
	}

	operator LPSTR() const throw()
	{
		return( m_psz );
	}

	LPSTR m_psz;

private:	// запрет некоторых операций
	CA2UTF8( const CA2UTF8& ) throw();
	CA2UTF8& operator=( const CA2UTF8& ) throw();
};

#define MAKEUTF8(x) \
	CA2UTF8 u8##x(x);

#endif

#endif


	// вспомогательные средства для конвертации UTF8 в CString
	// полезно в тех случаях, когда недоступен класс CA2CT(x,CP_UTF8)
	class CUTF82T
	{
	public:
		CUTF82T(LPCSTR u8):m_psz(NULL)
		{
			int len=strlen(u8)+1;
#if defined(_UNICODE)
			m_psz=new TCHAR[len];
			MultiByteToWideChar(CP_UTF8,0,u8,-1,m_psz,len);
#else
			LPWSTR r=LPWSTR(_alloca(len*2));
			MultiByteToWideChar(CP_UTF8,0,u8,-1,r,len);
			m_psz=new TCHAR[len];
			WideCharToMultiByte(CP_ACP,0,r,-1,m_psz,len,NULL,NULL);
#endif
		}

		~CUTF82T() throw()
		{
			if(m_psz)
				delete[]m_psz;
		}

		operator LPTSTR() const throw()
		{
			return( m_psz );
		}

		LPTSTR m_psz;

	private:	// запрет некоторых операций
		CUTF82T( const CUTF82T& ) throw();
		CUTF82T& operator=( const CUTF82T& ) throw();
	};

	class connection
	{
		sqlite3 *db;
	public:
		connection(LPCTSTR filename):db(NULL){
			MAKEUTF8(filename);
			int rc=sqlite3_open(u8filename,&db);
			if(rc!=SQLITE_OK){
				throw exception(rc);
			}
		}
		~connection(){
			sqlite3_close(db);
		}

		operator sqlite3*(){return db;}

		void exec(LPCTSTR sql){
			MAKEUTF8(sql);
			LPSTR errmsg=NULL;
			int rc=sqlite3_exec(db,u8sql,NULL,NULL,&errmsg);
			if(rc!=SQLITE_OK){
				throw exception(rc,errmsg);
			}
		}
		sqlite_int64 last_insert_rowid()
		{
			return sqlite3_last_insert_rowid(db);
		}
	};

	class statement
	{
		sqlite3_stmt *stmt;
		void check(int rc){
			if(rc!=SQLITE_OK){
				throw exception(rc);
			}
		}
	public:
		statement(sqlite3*db,LPCTSTR sql):stmt(NULL){
			MAKEUTF8(sql);
			LPCSTR tail;
			check(sqlite3_prepare_v2(db,u8sql,-1,&stmt,&tail));
		}
		void bind(int p){
			check(sqlite3_bind_null(stmt,p));
		}
		void bind(int p,int v){
			check(sqlite3_bind_int(stmt,p,v));
		}
		void bind(int p,unsigned long v){
			check(sqlite3_bind_int(stmt,p,v));
		}
		void bind(int p,unsigned __int32 v){
			check(sqlite3_bind_int(stmt,p,v));
		}
		void bind(int p,unsigned __int64 v){
			check(sqlite3_bind_int64(stmt,p,v));
		}
		void bind(int p,sqlite3_int64 v){
			check(sqlite3_bind_int64(stmt,p,v));
		}
		void bind(int p,double v){
			check(sqlite3_bind_double(stmt,p,v));
		}
		void bind(int p,LPCSTR v){
			MAKEUTF8(v);
			check(sqlite3_bind_text(stmt,p,u8v,-1,SQLITE_STATIC));
		}
		void bind(int p,LPCWSTR v){
			check(sqlite3_bind_text16(stmt,p,v,-1,SQLITE_STATIC));
		}
		void bind(int p,const void*pdata,int n){
			check(sqlite3_bind_blob(stmt,p,pdata,n,SQLITE_STATIC));
		}
// _bstr_t объявлен в файле comutil.h
#ifdef _INC_COMUTIL
		void bind(int p,_bstr_t const&v){
			check(sqlite3_bind_text16(stmt,p,(wchar_t*)v,-1,SQLITE_STATIC));
		}
#endif
		bool step(){
			int rc=sqlite3_step(stmt);
			switch(rc){
			case SQLITE_ROW:
				return true;
			case SQLITE_DONE:
				return false;
			default:
				throw exception(rc);
			}
		}
		int column_count(){
			return sqlite3_column_count(stmt);
		}
		int column_type(int p){
			return sqlite3_column_type(stmt,p);
		}
		int column_int(int p){
			return sqlite3_column_int(stmt,p);
		}
		sqlite3_int64 column_int64(int p){
			return sqlite3_column_int64(stmt,p);
		}
		double column_double(int p){
			return sqlite3_column_double(stmt,p);
		}
		CString column_text(int p){
			LPCSTR u8=LPCSTR(sqlite3_column_text(stmt,p));
			int len=sqlite3_column_bytes(stmt,p)+1;
			LPWSTR r=LPWSTR(_alloca(len*2));
			MultiByteToWideChar(CP_UTF8,0,u8,-1,r,len);
#if !defined(_UNICODE)
			LPSTR s=LPTSTR(_alloca(len));
			WideCharToMultiByte(CP_ACP,0,r,-1,s,len,NULL,NULL);
			return CString(s);
#else
			return CString(r);
#endif
		}
		const void*column_blob(int p){
			return sqlite3_column_blob(stmt,p);
		}
		int column_bytes(int p){
			return sqlite3_column_bytes(stmt,p);
		}
		void reset(){
			check(sqlite3_reset(stmt));
		}
		~statement(){
			sqlite3_finalize(stmt);
		}
	};

	class transaction
	{
		statement begin;
		statement commit;
		statement rollback;
	public:
		bool success;
		transaction(sqlite3*db):
			begin(db,TEXT("BEGIN TRANSACTION")),
			commit(db,TEXT("COMMIT TRANSACTION")),
			rollback(db,TEXT("ROLLBACK TRANSACTION")),
			success(false)
		{
			begin.step();
			begin.reset();
		}
		~transaction()
		{
			if(success)
				commit.step();
			else
				rollback.step();
		}
		void commitandbegin()
		{
			commit.step();
			commit.reset();
			begin.step();
			begin.reset();
		}
	};

};

