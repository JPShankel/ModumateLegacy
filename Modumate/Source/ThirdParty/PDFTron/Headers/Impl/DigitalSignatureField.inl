inline DigitalSignatureField::DigitalSignatureField(const DigitalSignatureField& other)
	: m_impl(other.m_impl)
{
}

inline DigitalSignatureField& DigitalSignatureField::operator= (const DigitalSignatureField& other)
{
	m_impl = other.m_impl;
	return *this;
}

inline DigitalSignatureField::DigitalSignatureField(TRN_DigitalSignatureField impl)
	: m_impl(impl)
{}

inline DigitalSignatureField::DigitalSignatureField(const PDF::Field& in_field)
{
	REX(TRN_DigitalSignatureFieldCreateFromField(&in_field.mp_field, &m_impl));
}

inline bool DigitalSignatureField::HasCryptographicSignature() const
{
	TRN_Bool result = 0;
	REX(TRN_DigitalSignatureFieldHasCryptographicSignature(&m_impl, &result));
	return result != 0;
}

inline DigitalSignatureField::SubFilterType DigitalSignatureField::GetSubFilter() const
{
	SubFilterType result;
	REX(TRN_DigitalSignatureFieldGetSubFilter(&m_impl, (enum TRN_DigitalSignatureField_SubFilterType*) &result));
	return result;
}

inline UString DigitalSignatureField::GetSignatureName() const
{
	TRN_UString result;
	REX(TRN_DigitalSignatureFieldGetSignatureName(&m_impl, &result));
	return UString(result);
}

inline Date DigitalSignatureField::GetSigningTime() const
{
	TRN_Date result;
	REX(TRN_DigitalSignatureFieldGetSigningTime(&m_impl, &result));
	return *((Date*)&result);
}

inline UString DigitalSignatureField::GetLocation() const
{
	TRN_UString result;
	REX(TRN_DigitalSignatureFieldGetLocation(&m_impl, &result));
	return UString(result);
}

inline UString DigitalSignatureField::GetReason() const
{
	TRN_UString result;
	REX(TRN_DigitalSignatureFieldGetReason(&m_impl, &result));
	return UString(result);
}

inline UString DigitalSignatureField::GetContactInfo() const
{
	TRN_UString result;
	REX(TRN_DigitalSignatureFieldGetContactInfo(&m_impl, &result));
	return UString(result);
}

inline std::vector<unsigned char> DigitalSignatureField::GetCert(UInt32 in_index) const {
	TRN_Vector cvector;
	REX(TRN_DigitalSignatureFieldGetCert(&m_impl, in_index, &cvector));
	void* arr;
	UInt32 size;
	REX(TRN_VectorGetData(cvector, &arr));
	REX(TRN_VectorGetSize(cvector, &size));
	std::vector<unsigned char> result(size);
	memcpy(&result[0], arr, size);
	TRN_VectorDestroy(cvector);
	return result;
}

inline UInt32 DigitalSignatureField::GetCertCount() const
{
	UInt32 result = 0;
	REX(TRN_DigitalSignatureFieldGetCertCount(&m_impl, &result));
	return result;
}

inline bool DigitalSignatureField::HasVisibleAppearance() const
{
	TRN_Bool result = 0;
	REX(TRN_DigitalSignatureFieldHasVisibleAppearance(&m_impl, &result));
	return result != 0;
}

inline void DigitalSignatureField::SetContactInfo(const UString& in_contact_info)
{
	REX(TRN_DigitalSignatureFieldSetContactInfo(&m_impl, in_contact_info.mp_impl));
}

inline void DigitalSignatureField::SetLocation(const UString& in_location)
{
	REX(TRN_DigitalSignatureFieldSetLocation(&m_impl, in_location.mp_impl));
}

inline void DigitalSignatureField::SetReason(const UString& in_reason)
{
	REX(TRN_DigitalSignatureFieldSetReason(&m_impl, in_reason.mp_impl));
}

// We use an std::vector of UTF-8 std::strings for SWIG, because SWIG has trouble with mapping UString to string when it's in a vector<UString>.
#ifdef SWIG
inline void DigitalSignatureField::SetFieldPermissions(const FieldPermissions in_action, const std::vector<std::string>& in_field_names)
{
	if (in_field_names.size() > 0)
	{
		std::vector<UString> tmp;
		tmp.reserve(in_field_names.size());
		for (std::vector<std::string>::const_iterator it = in_field_names.begin(); it != in_field_names.end(); ++it)
		{
			tmp.push_back(UString(*it));
		}
		TRN_UString* tmp2 = (TRN_UString*) &tmp[0];
		REX(TRN_DigitalSignatureFieldSetFieldPermissions(&m_impl, (enum TRN_DigitalSignatureField_FieldPermissions) in_action, tmp2, (TRN_UInt32)in_field_names.size()));
	}
	else // If the container is empty, the returned iterator value shall not be dereferenced.
	{
		REX(TRN_DigitalSignatureFieldSetFieldPermissions(&m_impl, (enum TRN_DigitalSignatureField_FieldPermissions) in_action, NULL, 0));
	}
}
#else
inline void DigitalSignatureField::SetFieldPermissions(const FieldPermissions in_action, const std::vector<UString>& in_field_names)
{
	// UString class contains only a TRN_UString and no virtual functions, so an array of UStrings is also an array of TRN_UStrings
	TRN_UString* tmp = in_field_names.size() > 0? (TRN_UString*) &in_field_names[0] : NULL;
	REX(TRN_DigitalSignatureFieldSetFieldPermissions(&m_impl, (enum TRN_DigitalSignatureField_FieldPermissions) in_action, tmp, (TRN_UInt32)in_field_names.size()));
}
#endif

inline void DigitalSignatureField::SetDocumentPermissions(DigitalSignatureField::DocumentPermissions in_perms)
{
	REX(TRN_DigitalSignatureFieldSetDocumentPermissions(&m_impl, (enum TRN_DigitalSignatureField_DocumentPermissions) in_perms));
}

inline void DigitalSignatureField::SignOnNextSave(const UString& in_pkcs12_keyfile_path, const UString& in_password)
{
	REX(TRN_DigitalSignatureFieldSignOnNextSave(&m_impl, in_pkcs12_keyfile_path.mp_impl, in_password.mp_impl));
}

inline void DigitalSignatureField::SignOnNextSave(const std::vector<unsigned char>& in_pkcs12_buffer, const UString& in_password)
{
	TRN_UInt8* tmp = in_pkcs12_buffer.size() > 0? (TRN_UInt8*) &in_pkcs12_buffer[0] : NULL;
	REX(TRN_DigitalSignatureFieldSignOnNextSaveFromBuffer(&m_impl, tmp, (TRN_UInt32) in_pkcs12_buffer.size(), in_password.mp_impl));
}

inline void DigitalSignatureField::SignOnNextSaveWithCustomHandler(const SDF::SignatureHandlerId in_signature_handler_id)
{
	REX(TRN_DigitalSignatureFieldSignOnNextSaveWithCustomHandler(&m_impl, (const TRN_SignatureHandlerId) in_signature_handler_id));
}

inline void DigitalSignatureField::CertifyOnNextSave(const UString& in_pkcs12_keyfile_path, const UString& in_password)
{
	REX(TRN_DigitalSignatureFieldCertifyOnNextSave(&m_impl, in_pkcs12_keyfile_path.mp_impl, in_password.mp_impl));
}

inline void DigitalSignatureField::CertifyOnNextSave(const std::vector<unsigned char>& in_pkcs12_buffer, const UString& in_password)
{
	TRN_UInt8* tmp = in_pkcs12_buffer.size() > 0? (TRN_UInt8*) &in_pkcs12_buffer[0] : NULL;
	REX(TRN_DigitalSignatureFieldCertifyOnNextSaveFromBuffer(&m_impl, tmp, (TRN_UInt32) in_pkcs12_buffer.size(), in_password.mp_impl));
}

inline void DigitalSignatureField::CertifyOnNextSaveWithCustomHandler(const SDF::SignatureHandlerId in_signature_handler_id)
{
	REX(TRN_DigitalSignatureFieldCertifyOnNextSaveWithCustomHandler(&m_impl, (const TRN_SignatureHandlerId) in_signature_handler_id));
}

inline SDF::Obj DigitalSignatureField::GetSDFObj() const
{
	TRN_Obj result;
	REX(TRN_DigitalSignatureFieldGetSDFObj(&m_impl,&result));
	return SDF::Obj(result);
}

inline bool DigitalSignatureField::IsLockedByDigitalSignature() const
{
	TRN_Bool result = 0;
	REX(TRN_DigitalSignatureFieldIsLockedByDigitalSignature(&m_impl, &result));
	return result != 0;
}

// We use an std::vector of UTF-8 std::strings for SWIG, because SWIG has trouble with mapping UString to string when it's in a vector<UString>.
#ifdef SWIG
inline std::vector<std::string> DigitalSignatureField::GetLockedFields() const
{
	// Broadly similar to PDFDraw::GetSeparationBitmaps implementation
	TRN_Vector fields_vec = NULL;
	REX(TRN_DigitalSignatureFieldGetLockedFields(&m_impl, (TRN_Vector*)&fields_vec));
	
	TRN_UInt32 fields_vec_size;
	TRN_VectorGetSize(fields_vec, &fields_vec_size);
	
	std::vector<std::string> result;
	for (TRN_UInt32 i = 0; i < fields_vec_size; i++)
	{
		TRN_UString current_ustr = NULL;
		TRN_VectorGetAt(fields_vec, i, (void**)&current_ustr);
		
		result.push_back(UString(current_ustr).ConvertToUtf8());
	}

	TRN_VectorDestroyKeepContents(fields_vec);
	return result;
}
#else
inline std::vector<UString> DigitalSignatureField::GetLockedFields() const
{
	// Broadly similar to PDFDraw::GetSeparationBitmaps implementation
	TRN_Vector fields_vec = NULL;
	REX(TRN_DigitalSignatureFieldGetLockedFields(&m_impl, (TRN_Vector*)&fields_vec));
	
	TRN_UInt32 fields_vec_size;
	TRN_VectorGetSize(fields_vec, &fields_vec_size);
	
	std::vector<UString> result;
	for (TRN_UInt32 i = 0; i < fields_vec_size; i++)
	{
		TRN_UString current_ustr = NULL;
		TRN_VectorGetAt(fields_vec, i, (void**)&current_ustr);
		result.push_back(UString(current_ustr));
	}

	TRN_VectorDestroyKeepContents(fields_vec);
	return result;
}
#endif

inline DigitalSignatureField::DocumentPermissions DigitalSignatureField::GetDocumentPermissions() const
{
	DigitalSignatureField::DocumentPermissions result;
	REX(TRN_DigitalSignatureFieldGetDocumentPermissions(&m_impl, (enum TRN_DigitalSignatureField_DocumentPermissions*) &result));
	return result;
}

inline void DigitalSignatureField::ClearSignature()
{
	REX(TRN_DigitalSignatureFieldClearSignature(&m_impl));
}

