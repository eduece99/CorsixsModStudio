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

#include "ToolRegistry.h"
#include "common/Utility.h"

ToolRegistry::~ToolRegistry()
{
    for (auto *pTool : m_vTools)
    {
        delete pTool;
    }
}

void ToolRegistry::Register(ITool *pTool) { m_vTools.push_back(pTool); }

size_t ToolRegistry::GetCount() const { return m_vTools.size(); }

ITool *ToolRegistry::Get(size_t i) const { return m_vTools[i]; }

void ToolRegistry::LaunchTool(size_t index)
{
    if (index < m_vTools.size())
    {
        m_vTools[index]->DoAction();
    }
}

void ToolRegistry::DoTool(const wxString &sName)
{
    for (auto *pTool : m_vTools)
    {
        std::cout << "Tool: " << pTool->GetName().ToStdString() << " vs " << sName.ToStdString() << std::endl;
        if (pTool->GetName() == sName)
        {
            pTool->DoAction();
            return;
        }
    }
    ErrorBox("Tool description mismatch");
}

void ToolRegistry::DoToolIdx(const int index)
{
    if (index >= 0 && index < static_cast<int>(m_vTools.size()))
    {
        m_vTools[index]->DoAction();
        return;
    }   
    ErrorBox("Tool description mismatch - invalid index");
}
