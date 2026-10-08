#pragma once

#include <wx/bitmap.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

namespace CDMSBitmapResources
{
inline wxBitmap LoadBitmap(const wxString &sResourceName, const wxString &sFilename)
{
#ifdef _WIN32
    return wxBitmap(sResourceName, wxBITMAP_TYPE_BMP_RESOURCE);
#else
    const wxString sPath =
        wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPathWithSep() + sFilename;
    return wxBitmap(sPath, wxBITMAP_TYPE_BMP);
#endif
}
} // namespace CDMSBitmapResources
