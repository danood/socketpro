
#include "mystruct.h"

SPA::CUQueue& operator <<(SPA::CUQueue &mc, const CMyStruct &ms) {
    ms.SaveTo(mc);
    return mc;
}

SPA::CUQueue& operator >>(SPA::CUQueue &mc, CMyStruct &ms) {
    ms.LoadFrom(mc);
    return mc;
}

void SetMyStruct(CMyStruct &ms) {
    ms.ObjBool = true;
    ms.UnicodeString = L"Unicode";
    ms.ABool = true;
    ms.ADouble = 1234.567;
    ms.AsciiString = "ASCII";
#ifdef WIN32_64
    ms.ObjString = L"test";
    {
        int *data;
        SAFEARRAYBOUND sab[1] = {2, 0};
        ms.objArrInt.vt = (VT_ARRAY | VT_INT);
        ms.objArrInt.parray = ::SafeArrayCreate(VT_INT, 1, sab);
        ::SafeArrayAccessData(ms.objArrInt.parray, (void**) &data);
        data[0] = 1;
        data[1] = 76890;
        ::SafeArrayUnaccessData(ms.objArrInt.parray);
    }
    {
        BSTR *data;
        SAFEARRAYBOUND sab[1] = {2, 0};
        ms.objArrString.vt = (VT_ARRAY | VT_BSTR);
        ms.objArrString.parray = ::SafeArrayCreate(VT_BSTR, 1, sab);
        ::SafeArrayAccessData(ms.objArrString.parray, (void**) &data);
        data[0] = ::SysAllocString(L"Hello");
        data[1] = ::SysAllocString(L"world");
        ::SafeArrayUnaccessData(ms.objArrString.parray);
    }
#else
    ms.ObjString = std::wstring(L"test");
    {
        std::vector<int> vs;
        vs.push_back(1);
        vs.push_back(76890);
        ms.objArrInt = vs;
    }
    {

        std::vector<std::wstring> vs;
        vs.push_back(std::wstring(L"Hello"));
        vs.push_back(std::wstring(L"world"));
        ms.objArrString = vs;
    }
#endif
}