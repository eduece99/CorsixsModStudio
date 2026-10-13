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
#include "views/frmRgdEditor.h"
#include "frame/Construct.h"
#include "common/Utility.h"
#include "actions/ActionUtil.h"
#include "common/strconv.h"
#include <rainman/formats/CRgdFile.h>
#include <memory>

class CRGDAction : public frmFiles::IHandler
{
  public:
    virtual wxString VGetExt() const { return wxT("rgd"); }

    virtual wxString VGetAction() const { return wxT("View as RGD file"); }

    virtual void VHandle(wxString sFile, wxTreeItemId &oParent, wxTreeItemId &oFile)
    {
        UNUSED(oFile);
        frmRGDEditor *pForm;
        TheConstruct->GetTabs()->AddPage(
            pForm = new frmRGDEditor(oParent, sFile, TheConstruct->GetTabs(), -1),
            wxString().Append(wxT("RGD")).Append(wxT(" [")).Append(OnlyFilename(sFile)).Append(wxT("]")), true);

        auto streamResult = TheConstruct->GetFileService().OpenStream(sFile);
        if (!streamResult)
        {
            ErrorBox("Cannot open file");
            return;
        }
        auto &stream = streamResult.value();

        auto pRgd = std::make_unique<CRgdFile>();
        pRgd->SetHashTable(TheConstruct->GetRgdHashTable());
        const auto sFilePath = wxStringToAscii(sFile);
        pRgd->Load(stream.get(), sFilePath.get());

        if (!pForm->FillFromMetaNode(pRgd.get()))
        {
            ErrorBox("Cannot load file");
            return;
        }
        pRgd.release(); // ownership transferred to form
    }
};
