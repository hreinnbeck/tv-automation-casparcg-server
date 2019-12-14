// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.
//
// ---------------------------------------------------------------------------
//
// This file was generated by the CEF translator tool. If making changes by
// hand only do so within the body of existing method and function
// implementations. See the translator.README.txt file in the tools directory
// for more information.
//
// $hash=cbb1645e96b0f389861ab2d00e2af9931ea416c2$
//

#include "libcef_dll/ctocpp/urlrequest_ctocpp.h"
#include "libcef_dll/cpptoc/urlrequest_client_cpptoc.h"
#include "libcef_dll/ctocpp/request_context_ctocpp.h"
#include "libcef_dll/ctocpp/request_ctocpp.h"
#include "libcef_dll/ctocpp/response_ctocpp.h"

// STATIC METHODS - Body may be edited by hand.

NO_SANITIZE("cfi-icall")
CefRefPtr<CefURLRequest> CefURLRequest::Create(
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefURLRequestClient> client,
    CefRefPtr<CefRequestContext> request_context) {
  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  // Verify param: request; type: refptr_same
  DCHECK(request.get());
  if (!request.get())
    return NULL;
  // Verify param: client; type: refptr_diff
  DCHECK(client.get());
  if (!client.get())
    return NULL;
  // Unverified params: request_context

  // Execute
  cef_urlrequest_t* _retval =
      cef_urlrequest_create(CefRequestCToCpp::Unwrap(request),
                            CefURLRequestClientCppToC::Wrap(client),
                            CefRequestContextCToCpp::Unwrap(request_context));

  // Return type: refptr_same
  return CefURLRequestCToCpp::Wrap(_retval);
}

// VIRTUAL METHODS - Body may be edited by hand.

NO_SANITIZE("cfi-icall")
CefRefPtr<CefRequest> CefURLRequestCToCpp::GetRequest() {
  cef_urlrequest_t* _struct = GetStruct();
  if (CEF_MEMBER_MISSING(_struct, get_request))
    return NULL;

  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  // Execute
  cef_request_t* _retval = _struct->get_request(_struct);

  // Return type: refptr_same
  return CefRequestCToCpp::Wrap(_retval);
}

NO_SANITIZE("cfi-icall")
CefRefPtr<CefURLRequestClient> CefURLRequestCToCpp::GetClient() {
  cef_urlrequest_t* _struct = GetStruct();
  if (CEF_MEMBER_MISSING(_struct, get_client))
    return NULL;

  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  // Execute
  cef_urlrequest_client_t* _retval = _struct->get_client(_struct);

  // Return type: refptr_diff
  return CefURLRequestClientCppToC::Unwrap(_retval);
}

NO_SANITIZE("cfi-icall")
CefURLRequest::Status CefURLRequestCToCpp::GetRequestStatus() {
  cef_urlrequest_t* _struct = GetStruct();
  if (CEF_MEMBER_MISSING(_struct, get_request_status))
    return UR_UNKNOWN;

  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  // Execute
  cef_urlrequest_status_t _retval = _struct->get_request_status(_struct);

  // Return type: simple
  return _retval;
}

NO_SANITIZE("cfi-icall")
CefURLRequest::ErrorCode CefURLRequestCToCpp::GetRequestError() {
  cef_urlrequest_t* _struct = GetStruct();
  if (CEF_MEMBER_MISSING(_struct, get_request_error))
    return ERR_NONE;

  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  // Execute
  cef_errorcode_t _retval = _struct->get_request_error(_struct);

  // Return type: simple
  return _retval;
}

NO_SANITIZE("cfi-icall")
CefRefPtr<CefResponse> CefURLRequestCToCpp::GetResponse() {
  cef_urlrequest_t* _struct = GetStruct();
  if (CEF_MEMBER_MISSING(_struct, get_response))
    return NULL;

  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  // Execute
  cef_response_t* _retval = _struct->get_response(_struct);

  // Return type: refptr_same
  return CefResponseCToCpp::Wrap(_retval);
}

NO_SANITIZE("cfi-icall") bool CefURLRequestCToCpp::ResponseWasCached() {
  cef_urlrequest_t* _struct = GetStruct();
  if (CEF_MEMBER_MISSING(_struct, response_was_cached))
    return false;

  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  // Execute
  int _retval = _struct->response_was_cached(_struct);

  // Return type: bool
  return _retval ? true : false;
}

NO_SANITIZE("cfi-icall") void CefURLRequestCToCpp::Cancel() {
  cef_urlrequest_t* _struct = GetStruct();
  if (CEF_MEMBER_MISSING(_struct, cancel))
    return;

  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  // Execute
  _struct->cancel(_struct);
}

// CONSTRUCTOR - Do not edit by hand.

CefURLRequestCToCpp::CefURLRequestCToCpp() {}

template <>
cef_urlrequest_t*
CefCToCppRefCounted<CefURLRequestCToCpp, CefURLRequest, cef_urlrequest_t>::
    UnwrapDerived(CefWrapperType type, CefURLRequest* c) {
  NOTREACHED() << "Unexpected class type: " << type;
  return NULL;
}

#if DCHECK_IS_ON()
template <>
base::AtomicRefCount
    CefCToCppRefCounted<CefURLRequestCToCpp, CefURLRequest, cef_urlrequest_t>::
        DebugObjCt ATOMIC_DECLARATION;
#endif

template <>
CefWrapperType CefCToCppRefCounted<CefURLRequestCToCpp,
                                   CefURLRequest,
                                   cef_urlrequest_t>::kWrapperType =
    WT_URLREQUEST;
