// this ALWAYS GENERATED file contains the definitions for the interfaces


// File created by MIDL compiler version 3.01.75
// at Wed Jun 06 18:20:37 2001
// Compiler settings for .\if_ole.idl:
//  Os (OptLev=s), W1, Zp8, env=Win32, ms_ext, c_ext
//  error checks: none
//@@MIDL_FILE_HEADING(  )
#include "rpc.h"
#include "rpcndr.h"
#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif //COM_NO_WINDOWS_H

#ifndef __if_ole_h__
#define __if_ole_h__

#ifdef __cplusplus
extern "C"{
#endif

// Forward Declarations

#ifndef __IMNV_FWD_DEFINED__
#define __IMNV_FWD_DEFINED__
typedef interface IMNV IMNV;
#endif	// __IMNV_FWD_DEFINED__


#ifndef __MNV_FWD_DEFINED__
#define __MNV_FWD_DEFINED__

#ifdef __cplusplus
typedef class MNV MNV;
#else
typedef struct MNV MNV;
#endif // __cplusplus

#endif	// __MNV_FWD_DEFINED__


// header files for imported files
#include "oaidl.h"

#ifndef __MIDL_user_allocate_free_DEFINED__
#define __MIDL_user_allocate_free_DEFINED__
    void __RPC_FAR * __RPC_USER MIDL_user_allocate(size_t);
    void __RPC_USER MIDL_user_free( void __RPC_FAR * );
#endif

#ifndef __IMNV_INTERFACE_DEFINED__
#define __IMNV_INTERFACE_DEFINED__

//***************************************
// Generated header for interface: IMNV
// at Wed Jun 06 18:20:37 2001
// using MIDL 3.01.75
//**************************************
// [oleautomation][dual][unique][helpstring][uuid][object]



EXTERN_C const IID IID_IMNV;

#if defined(__cplusplus) && !defined(CINTERFACE)

    interface DECLSPEC_UUID("0F0BFAE2-4C90-11d1-82D7-0004AC368519")
    IMNV : public IDispatch
    {
    public:
	virtual HRESULT STDMETHODCALLTYPE SendKeys(
	    /* [in] */ BSTR keys) = 0;

	virtual HRESULT STDMETHODCALLTYPE Eval(
	    /* [in] */ BSTR expr,
	    /* [retval][out] */ BSTR __RPC_FAR *result) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetForeground( void) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetHwnd(
	    /* [retval][out] */ UINT_PTR __RPC_FAR *result) = 0;

    };

#else	// C style interface

    typedef struct IMNVVtbl
    {
	BEGIN_INTERFACE

	HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )(
	    IMNV __RPC_FAR * This,
	    /* [in] */ REFIID riid,
	    /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);

	ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )(
	    IMNV __RPC_FAR * This);

	ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )(
	    IMNV __RPC_FAR * This);

	HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfoCount )(
	    IMNV __RPC_FAR * This,
	    /* [out] */ UINT __RPC_FAR *pctinfo);

	HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfo )(
	    IMNV __RPC_FAR * This,
	    /* [in] */ UINT iTInfo,
	    /* [in] */ LCID lcid,
	    /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo);

	HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetIDsOfNames )(
	    IMNV __RPC_FAR * This,
	    /* [in] */ REFIID riid,
	    /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames,
	    /* [in] */ UINT cNames,
	    /* [in] */ LCID lcid,
	    /* [size_is][out] */ DISPID __RPC_FAR *rgDispId);

	/* [local] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Invoke )(
	    IMNV __RPC_FAR * This,
	    /* [in] */ DISPID dispIdMember,
	    /* [in] */ REFIID riid,
	    /* [in] */ LCID lcid,
	    /* [in] */ WORD wFlags,
	    /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams,
	    /* [out] */ VARIANT __RPC_FAR *pVarResult,
	    /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo,
	    /* [out] */ UINT __RPC_FAR *puArgErr);

	HRESULT ( STDMETHODCALLTYPE __RPC_FAR *SendKeys )(
	    IMNV __RPC_FAR * This,
	    /* [in] */ BSTR keys);

	HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Eval )(
	    IMNV __RPC_FAR * This,
	    /* [in] */ BSTR expr,
	    /* [retval][out] */ BSTR __RPC_FAR *result);

	HRESULT ( STDMETHODCALLTYPE __RPC_FAR *SetForeground )(
	    IMNV __RPC_FAR * This);

	HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetHwnd )(
	    IMNV __RPC_FAR * This,
	    /* [retval][out] */ UINT_PTR __RPC_FAR *result);

	END_INTERFACE
    } IMNVVtbl;

    interface IMNV
    {
	CONST_VTBL struct IMNVVtbl __RPC_FAR *lpVtbl;
    };

#ifdef COBJMACROS


#define IMNV_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IMNV_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IMNV_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IMNV_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IMNV_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IMNV_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IMNV_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IMNV_SendKeys(This,keys)	\
    (This)->lpVtbl -> SendKeys(This,keys)

#define IMNV_Eval(This,expr,result)	\
    (This)->lpVtbl -> Eval(This,expr,result)

#define IMNV_SetForeground(This)	\
    (This)->lpVtbl -> SetForeground(This)

#define IMNV_GetHwnd(This,result)	\
    (This)->lpVtbl -> GetHwnd(This,result)

#endif // COBJMACROS


#endif	// C style interface



HRESULT STDMETHODCALLTYPE IMNV_SendKeys_Proxy(
    IMNV __RPC_FAR * This,
    /* [in] */ BSTR keys);


void __RPC_STUB IMNV_SendKeys_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IMNV_Eval_Proxy(
    IMNV __RPC_FAR * This,
    /* [in] */ BSTR expr,
    /* [retval][out] */ BSTR __RPC_FAR *result);


void __RPC_STUB IMNV_Eval_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IMNV_SetForeground_Proxy(
    IMNV __RPC_FAR * This);


void __RPC_STUB IMNV_SetForeground_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IMNV_GetHwnd_Proxy(
    IMNV __RPC_FAR * This,
    /* [retval][out] */ UINT_PTR __RPC_FAR *result);


void __RPC_STUB IMNV_GetHwnd_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif	// __IMNV_INTERFACE_DEFINED__



#ifndef __MNV_LIBRARY_DEFINED__
#define __MNV_LIBRARY_DEFINED__

//***************************************
// Generated header for library: MNV
// at Wed Jun 06 18:20:37 2001
// using MIDL 3.01.75
//**************************************
// [version][helpstring][uuid]



EXTERN_C const IID LIBID_MNV;

#ifdef __cplusplus
EXTERN_C const CLSID CLSID_MNV;

class DECLSPEC_UUID("0F0BFAE1-4C90-11d1-82D7-0004AC368519")
MNV;
#endif
#endif // __MNV_LIBRARY_DEFINED__

// Additional Prototypes for ALL interfaces

unsigned long		  __RPC_USER  BSTR_UserSize(	 unsigned long __RPC_FAR *, unsigned long	     , BSTR __RPC_FAR * );
unsigned char __RPC_FAR * __RPC_USER  BSTR_UserMarshal(  unsigned long __RPC_FAR *, unsigned char __RPC_FAR *, BSTR __RPC_FAR * );
unsigned char __RPC_FAR * __RPC_USER  BSTR_UserUnmarshal(unsigned long __RPC_FAR *, unsigned char __RPC_FAR *, BSTR __RPC_FAR * );
void			  __RPC_USER  BSTR_UserFree(	 unsigned long __RPC_FAR *, BSTR __RPC_FAR * );

// end of Additional Prototypes

#ifdef __cplusplus
}
#endif

#endif
