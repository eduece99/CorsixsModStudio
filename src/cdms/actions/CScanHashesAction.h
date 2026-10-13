/*
    This file is part of Corsix's Mod Studio.

    Corsix's Mod Studio is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Corsix's Mod Studio is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Corsix's Mod Studio; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#pragma once

#include "views/frmFiles.h"
#include "common/ThemeColours.h"
#include "frame/Construct.h"
#include "common/Utility.h"
#include "common/strconv.h"
#include "common/strings.h"
#include "common/config.h"
#include <rainman/formats/CRgdFile.h>
#include <memory>
#include <wx/progdlg.h>
#include <vector>

extern "C"
{
    typedef unsigned long int ub4;
    typedef unsigned char ub1;
    ub4 hash(ub1 *k, ub4 length, ub4 initval);
}

class CScanHashesAction : public frmFiles::IHandler
{
  protected:
    wxProgressDialog *m_pProgress;

  public:
    CScanHashesAction() : m_pProgress(0) {}
    virtual wxString VGetExt() const { return wxT(""); }

    virtual wxString VGetAction() const { return wxT("Scan RGDs in this folder for unknown hashes"); }

    virtual size_t Recurse(wxString sFolder, wxTreeItemId &oParent, bool bCountOnly, size_t iCountBase,
                           size_t iCountDiv)
    {
        size_t iCount = 0;
        wxTreeItemIdValue oCookie;
        wxTreeCtrl *pTree = TheConstruct->GetFilesList()->GetTree();
        wxTreeItemId oChild = pTree->GetFirstChild(oParent, oCookie);
        // const char* sThisMod = TheConstruct->GetModule()->GetFileMapName();
        while (oChild.IsOk())
        {
            CFilesTreeItemData *pData = (CFilesTreeItemData *)pTree->GetItemData(oChild);
            if (pData->sMod)
            {
                wxString sFile = pTree->GetItemText(oChild);
                if (sFile.AfterLast('.').IsSameAs(wxT("rgd"), false) ||
                    sFile.AfterLast('.').IsSameAs(wxT("bfx"), false))
                {
                    ++iCount;
                    if (!bCountOnly)
                    {
                        int iPVal = (int)((iCountBase + iCount) / iCountDiv);
                        m_pProgress->Update(iPVal, sFile);

                        sFile = sFolder + wxT("\\") + pTree->GetItemText(oChild);

                        auto streamResult = TheConstruct->GetFileService().OpenStream(sFile);
                        if (streamResult)
                        {
                            auto &stream = streamResult.value();
                            CRgdFile oRGD;
                            oRGD.SetHashTable(TheConstruct->GetRgdHashTable());
                            const auto sFilePath = wxStringToAscii(sFile);
                            oRGD.Load(stream.get(), sFilePath.get());
                        }
                    }
                }
            }
            else
            {
                wxString sNewPath = sFolder;
                sNewPath.Append('\\');
                sNewPath.Append(pTree->GetItemText(oChild));
                iCount += Recurse(sNewPath, oChild, bCountOnly, iCountBase + iCount, iCountDiv);
            }
            oChild = pTree->GetNextChild(oChild, oCookie);
        }
        if (!bCountOnly)
        {
            if (pTree->IsExpanded(oParent))
            {
                auto itrResult = TheConstruct->GetFileService().Iterate(sFolder);
                auto pDir =
                    itrResult ? std::unique_ptr<IDirectoryTraverser::IIterator>(itrResult.value().release()) : nullptr;
                TheConstruct->GetFilesList()->UpdateDirectoryChildren(oParent, pDir.get());
            }
        }
        return iCount;
    }

    virtual void VHandle(wxString sFile, wxTreeItemId &oParent, wxTreeItemId &oFile)
    {
        UNUSED(oFile);
        size_t iCount = Recurse(sFile, oParent, true, 0, 1);
        size_t iDiv = 1;
        while ((iCount / iDiv) > (size_t)0x200)
        {
            ++iDiv;
        }
        m_pProgress = new wxProgressDialog(VGetAction(), wxT(""), (int)(iCount / iDiv) + 1, TheConstruct,
                                           wxPD_SMOOTH | wxPD_AUTO_HIDE);
        Recurse(sFile, oParent, false, 0, iDiv);
        delete m_pProgress;

        wxFileDialog *pFileDialog = 0;
        pFileDialog = new wxFileDialog(TheConstruct, AppStr(mod_select_dll), ConfGetDoWFolder(), wxT(""),
                                       AppStr(mod_dll_filter), wxFD_OPEN);
        if (!pFileDialog)
        {
            return;
        }
        if (pFileDialog->ShowModal() == wxID_OK)
        {
            std::vector<unsigned long> oHashList;
            TheConstruct->GetRgdHashTable()->FillUnknownList(oHashList);

            auto sStr = std::make_unique<unsigned char[]>(4096);
            auto sStr2 = std::make_unique<unsigned char[]>(4096);
            unsigned long iHash;
            size_t iL = 0;
            unsigned char iByte;
            char *sFile = UnicodeToAscii(pFileDialog->GetPath());
            FILE *fIn = fopen(sFile, "rb");
            fseek(fIn, 0, SEEK_END);
            iCount = ftell(fIn);
            fseek(fIn, 0, SEEK_SET);
            size_t iDiv = 1;
            while ((iCount / iDiv) > (size_t)0x200)
            {
                ++iDiv;
            }
            m_pProgress = new wxProgressDialog(VGetAction(), wxT("Scanning file"), (int)(iCount / iDiv) + 2,
                                               TheConstruct, wxPD_SMOOTH | wxPD_AUTO_HIDE);
            iCount = 0;
            delete[] sFile;
            while (!feof(fIn))
            {
                if ((++iCount % iDiv) == 0)
                {
                    m_pProgress->Update((int)(iCount / iDiv), wxT("Scanning file"));
                }
                fread(&iByte, 1, 1, fIn);
                if (iByte > 31 && iByte < 128)
                {
                    sStr[iL] = iByte;
                    ++iL;
                }
                else
                {
                    if (iL)
                    {
                        sStr[iL] = 0;
                        for (size_t i = 0; i < iL; ++i)
                        {
                            for (size_t j = i; j < iL; ++j)
                            {
                                iHash = hash((ub1 *)(sStr.get() + i), (ub4)(j - i + 1), (ub4)0);
                                for (std::vector<unsigned long>::iterator itr = oHashList.begin();
                                     itr != oHashList.end(); ++itr)
                                {
                                    if (*itr == iHash)
                                    {
                                        sStr[j + 1] = 0;
                                        TheConstruct->GetRgdHashTable()->ValueToHash((const char *)sStr.get() + i);
                                        break;
                                    }
                                }
                            }
                        }
                        iL = 0;
                    }
                }
            }
            fclose(fIn);
            ThemeColours::ShowMessageBox(wxT("Scan complete"), VGetAction(), wxICON_INFORMATION, TheConstruct);
            delete m_pProgress;
        }
    }
};
