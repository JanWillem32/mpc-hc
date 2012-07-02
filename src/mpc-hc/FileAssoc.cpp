/*
 * $Id$
 *
 * (C) 2003-2006 Gabest
 * (C) 2006-2012 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include "FileAssoc.h"
#include "resource.h"
#include "WinAPIUtils.h"


// TODO: change this along with the root key for settings and the mutex name to
//       avoid possible risks of conflict with the old MPC (non HC version).
#ifdef _WIN64
#define PROGID _T("mplayerc64")
#else
#define PROGID _T("mplayerc")
#endif // _WIN64

LPCTSTR CFileAssoc::strRegisteredAppName  = _T("Media Player Classic");
LPCTSTR CFileAssoc::strOldAssocKey        = _T("PreviousRegistration");
LPCTSTR CFileAssoc::strRegisteredAppKey   = _T("Software\\Clients\\Media\\Media Player Classic\\Capabilities");
LPCTSTR CFileAssoc::strRegAppFileAssocKey = _T("Software\\Clients\\Media\\Media Player Classic\\Capabilities\\FileAssociations");

const CString CFileAssoc::strOpenCommand    = CFileAssoc::GetOpenCommand();
const CString CFileAssoc::strEnqueueCommand = CFileAssoc::GetEnqueueCommand();

CComPtr<IApplicationAssociationRegistration> CFileAssoc::m_pAAR;

CString CFileAssoc::m_iconLibPath;
HMODULE CFileAssoc::m_hIconLib = NULL;
CFileAssoc::GetIconIndexFunc CFileAssoc::GetIconIndex = NULL;

CString CFileAssoc::GetOpenCommand()
{
    return _T("\"") + GetProgramPath(true) + _T("\" \"%1\"");
}

CString CFileAssoc::GetEnqueueCommand()
{
    return _T("\"") + GetProgramPath(true) + _T("\" /add \"%1\"");
}

IApplicationAssociationRegistration* CFileAssoc::CreateRegistrationManager()
{
    IApplicationAssociationRegistration* pAAR = NULL;

    // Default manager (requires at least Vista)
    HRESULT hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration,
                                  NULL,
                                  CLSCTX_INPROC,
                                  __uuidof(IApplicationAssociationRegistration),
                                  (LPVOID*)&pAAR);
    UNREFERENCED_PARAMETER(hr);

    return pAAR;
}

bool CFileAssoc::LoadIconsLib()
{
    bool loaded = false;

    FreeIconsLib();

    m_iconLibPath = GetProgramPath() + _T("mpciconlib.dll");
    m_hIconLib = LoadLibrary(m_iconLibPath);
    if (m_hIconLib) {
        GetIconIndex = (GetIconIndexFunc)GetProcAddress(m_hIconLib, "get_icon_index");

        if (GetIconIndex) {
            loaded = true;
        } else {
            FreeIconsLib();
        }
    }

    return loaded;
}

bool CFileAssoc::FreeIconsLib()
{
    bool unloaded = false;

    if (m_hIconLib && FreeLibrary(m_hIconLib)) {
        unloaded = true;
        m_iconLibPath.Empty();
        m_hIconLib = NULL;
        GetIconIndex = NULL;
    }

    return unloaded;
}
bool CFileAssoc::RegisterApp()
{
    bool success = false;

    if (!m_pAAR) {
        m_pAAR = CFileAssoc::CreateRegistrationManager();
    }

    if (m_pAAR) {
        CString appIcon = "\"" + GetProgramPath(true) + "\",0";

        // Register MPC for the windows "Default application" manager
        CRegKey key;

        if (ERROR_SUCCESS == key.Open(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\RegisteredApplications"))) {
            key.SetStringValue(_T("Media Player Classic"), strRegisteredAppKey);

            if (ERROR_SUCCESS == key.Create(HKEY_LOCAL_MACHINE, strRegisteredAppKey)) {
                // ==>>  TODO icon !!!
                key.SetStringValue(_T("ApplicationDescription"), ResStr(IDS_APP_DESCRIPTION), REG_EXPAND_SZ);
                key.SetStringValue(_T("ApplicationIcon"), appIcon, REG_EXPAND_SZ);
                key.SetStringValue(_T("ApplicationName"), ResStr(IDR_MAINFRAME), REG_EXPAND_SZ);

                success = true;
            }
        }
    }

    return success;
}

bool CFileAssoc::Register(CString ext, CString strLabel, bool bRegister, bool bRegisterContextMenuEntries, bool bAssociatedWithIcon)
{
    CRegKey key;
    CString strProgID = PROGID + ext;

    if (!bRegister) {
        if (bRegister != IsRegistered(ext)) {
            SetFileAssociation(ext, strProgID, bRegister);
        }
        key.Attach(HKEY_CLASSES_ROOT);
        key.RecurseDeleteKey(strProgID);

        return true;
    } else {
        // Create ProgID for this file type
        if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strProgID)
                || ERROR_SUCCESS != key.SetStringValue(NULL, strLabel)) {
            return false;
        }

        // Add to playlist option
        if (bRegisterContextMenuEntries) {
            if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strProgID + _T("\\shell\\enqueue"))
                    || ERROR_SUCCESS != key.SetStringValue(NULL, ResStr(IDS_ADD_TO_PLAYLIST))
                    || ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strProgID + _T("\\shell\\enqueue\\command"))
                    || ERROR_SUCCESS != key.SetStringValue(NULL, strEnqueueCommand)) {
                return false;
            }
        } else {
            key.Close();
            key.Attach(HKEY_CLASSES_ROOT);
            key.RecurseDeleteKey(strProgID + _T("\\shell\\enqueue"));
        }

        // Play option
        if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strProgID + _T("\\shell\\open"))) {
            return false;
        }
        if (bRegisterContextMenuEntries) {
            if (ERROR_SUCCESS != key.SetStringValue(NULL, ResStr(IDS_OPEN_WITH_MPC))) {
                return false;
            }
        } else {
            if (ERROR_SUCCESS != key.SetStringValue(NULL, _T(""))) {
                return false;
            }
        }

        if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strProgID + _T("\\shell\\open\\command"))
                || ERROR_SUCCESS != key.SetStringValue(NULL, strOpenCommand)) {
            return false;
        }

        if (ERROR_SUCCESS != key.Create(HKEY_LOCAL_MACHINE, strRegAppFileAssocKey)
                || key.SetStringValue(ext, strProgID)) {
            return false;
        }

        if (bAssociatedWithIcon) {
            CString appIcon;

            if (m_hIconLib) {
                int iconIndex = GetIconIndex(ext);
                CString typeicon = m_iconLibPath;

                /* icon_index value -1 means no icon was found in the iconlib for the file extension */
                if (iconIndex >= 0 && ExtractIcon(AfxGetApp()->m_hInstance, m_iconLibPath, iconIndex)) {
                    appIcon.Format(_T("\"%s\",%d"), m_iconLibPath, iconIndex);
                }
            }

            /* no icon was found for the file extension, so use MPC's icon */
            if (appIcon.IsEmpty()) {
                appIcon = "\"" + GetProgramPath(true) + "\",0";
            }

            if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strProgID + _T("\\DefaultIcon"))
                    || ERROR_SUCCESS != key.SetStringValue(NULL, appIcon)) {
                return false;
            }
        } else {
            key.Attach(HKEY_CLASSES_ROOT);
            key.RecurseDeleteKey(strProgID + _T("\\DefaultIcon"));
        }

        if (bRegister != IsRegistered(ext)) {
            SetFileAssociation(ext, strProgID, bRegister);
        }

        return true;
    }
}

bool CFileAssoc::SetFileAssociation(CString strExt, CString strProgID, bool bRegister)
{
    CString extOldReg, extOldIcon;
    CRegKey key;
    HRESULT hr = S_OK;
    TCHAR   buff[_MAX_PATH];
    ULONG   len = _countof(buff);
    memset(buff, 0, sizeof(buff));

    if (!m_pAAR) {
        m_pAAR = CFileAssoc::CreateRegistrationManager();
    }

    if (m_pAAR) {
        // The Vista way
        CString strNewApp;
        if (bRegister) {
            // Create non existing file type
            if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strExt)) {
                return false;
            }

            LPTSTR pszCurrentAssociation;
            // Save the application currently associated
            if (SUCCEEDED(m_pAAR->QueryCurrentDefault(strExt, AT_FILEEXTENSION, AL_EFFECTIVE, &pszCurrentAssociation))) {
                if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strProgID)) {
                    return false;
                }

                key.SetStringValue(strOldAssocKey, pszCurrentAssociation);

                /*
                // Get current icon for file type
                if (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, CString(pszCurrentAssociation) + _T("\\DefaultIcon")))
                {
                    len = sizeof(buff);
                    memset(buff, 0, len);
                    if (ERROR_SUCCESS == key.QueryStringValue(NULL, buff, &len) && !CString(buff).Trim().IsEmpty())
                    {
                        if (ERROR_SUCCESS == key.Create(HKEY_CLASSES_ROOT, strProgID + _T("\\DefaultIcon")))
                            key.SetStringValue (NULL, buff);
                    }
                }
                */
                CoTaskMemFree(pszCurrentAssociation);
            }
            strNewApp = strRegisteredAppName;
        } else {
            if (ERROR_SUCCESS != key.Open(HKEY_CLASSES_ROOT, strProgID)) {
                return false;
            }

            if (ERROR_SUCCESS == key.QueryStringValue(strOldAssocKey, buff, &len)) {
                strNewApp = buff;
            }

            // TODO : retrieve registered app name from previous association (or find Bill function for that...)
        }

        hr = m_pAAR->SetAppAsDefault(strNewApp, strExt, AT_FILEEXTENSION);
    } else {
        // The 2000/XP way
        if (bRegister) {
            // Set new association
            if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strExt)) {
                return false;
            }

            len = _countof(buff);
            memset(buff, 0, sizeof(buff));
            if (ERROR_SUCCESS == key.QueryStringValue(NULL, buff, &len) && !CString(buff).Trim().IsEmpty()) {
                extOldReg = buff;
            }
            if (ERROR_SUCCESS != key.SetStringValue(NULL, strProgID)) {
                return false;
            }

            /*
            // Get current icon for file type
            if (!extOldReg.IsEmpty())
            {
                if (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, extoldreg + _T("\\DefaultIcon")))
                {
                    len = sizeof(buff);
                    memset(buff, 0, len);
                    if (ERROR_SUCCESS == key.QueryStringValue(NULL, buff, &len) && !CString(buff).Trim().IsEmpty())
                        extOldIcon = buff;
                }
            }
            */

            // Save old association
            if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strProgID)) {
                return false;
            }
            key.SetStringValue(strOldAssocKey, extOldReg);

            /*
            if (!extOldIcon.IsEmpty() && (ERROR_SUCCESS == key.Create(HKEY_CLASSES_ROOT, strProgID + _T("\\DefaultIcon"))))
                key.SetStringValue(NULL, extOldIcon);
            */
        } else {
            // Get previous association
            len = _countof(buff);
            memset(buff, 0, sizeof(buff));
            if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strProgID)) {
                return false;
            }
            if (ERROR_SUCCESS == key.QueryStringValue(strOldAssocKey, buff, &len) && !CString(buff).Trim().IsEmpty()) {
                extOldReg = buff;
            }

            // Set previous association
            if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strExt)) {
                return false;
            }
            key.SetStringValue(NULL, extOldReg);
        }
    }

    return SUCCEEDED(hr);
}

bool CFileAssoc::IsRegistered(CString ext)
{
    BOOL    bIsDefault = FALSE;
    CString strProgID = PROGID + ext;

    if (!m_pAAR) {
        m_pAAR = CFileAssoc::CreateRegistrationManager();
    }

    if (m_pAAR) {
        // The Vista way
        m_pAAR->QueryAppIsDefault(ext, AT_FILEEXTENSION, AL_EFFECTIVE, strRegisteredAppName, &bIsDefault);
    } else {
        // The 2000/XP way
        CRegKey key;
        TCHAR   buff[_MAX_PATH];
        ULONG   len = _countof(buff);
        memset(buff, 0, sizeof(buff));

        if (ERROR_SUCCESS != key.Open(HKEY_CLASSES_ROOT, ext, KEY_READ)
                || ERROR_SUCCESS != key.QueryStringValue(NULL, buff, &len)
                || CString(buff).Trim().IsEmpty()) {
            return false;
        }

        bIsDefault = (buff == strProgID);
    }

    // Check if association is for this instance of MPC
    if (bIsDefault) {
        CRegKey key;
        TCHAR   buff[_MAX_PATH];
        ULONG   len = _countof(buff);

        bIsDefault = FALSE;
        if (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, strProgID + _T("\\shell\\open\\command"), KEY_READ)) {
            if (ERROR_SUCCESS == key.QueryStringValue(NULL, buff, &len)) {
                bIsDefault = (strOpenCommand.CompareNoCase(CString(buff)) == 0);
            }
        }
    }

    return !!bIsDefault;
}

bool CFileAssoc::AreRegisteredFileContextMenuEntries(CString strExt)
{
    CRegKey key;
    TCHAR   buff[_MAX_PATH];
    ULONG   len = _countof(buff);
    CString strProgID = PROGID + strExt;
    bool    registered = false;

    if (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, strProgID + _T("\\shell\\open"), KEY_READ)) {
        CString strCommand = ResStr(IDS_OPEN_WITH_MPC);
        if (ERROR_SUCCESS == key.QueryStringValue(NULL, buff, &len)) {
            registered = (strCommand.CompareNoCase(CString(buff)) == 0);
        }
    }

    return registered;
}

bool CFileAssoc::Register(CMediaFormatCategory& mfc, bool bRegister, bool bRegisterContextMenuEntries, bool bAssociatedWithIcon)
{
    CAtlList<CString> exts;
    ExplodeMin(mfc.GetExtsWithPeriod(), exts, ' ');

    CString strLabel = mfc.GetDescription();
    bool res = true;

    POSITION pos = exts.GetHeadPosition();
    while (pos) {
        res &= Register(exts.GetNext(pos), strLabel, bRegister, bRegisterContextMenuEntries, bAssociatedWithIcon);
    }

    return res;
}

CFileAssoc::reg_state_t CFileAssoc::IsRegistered(const CMediaFormatCategory& mfc)
{
    CAtlList<CString> exts;
    ExplodeMin(mfc.GetExtsWithPeriod(), exts, ' ');

    size_t cnt = 0;

    POSITION pos = exts.GetHeadPosition();
    while (pos) {
        if (CFileAssoc::IsRegistered(exts.GetNext(pos))) {
            cnt++;
        }
    }

    reg_state_t res;
    if (cnt == 0) {
        res = NOT_REGISTERED;
    } else if (cnt == exts.GetCount()) {
        res = ALL_REGISTERED;
    } else {
        res = SOME_REGISTERED;
    }

    return res;
}

CFileAssoc::reg_state_t CFileAssoc::AreRegisteredFileContextMenuEntries(const CMediaFormatCategory& mfc)
{
    CAtlList<CString> exts;
    ExplodeMin(mfc.GetExtsWithPeriod(), exts, ' ');

    size_t cnt = 0;

    POSITION pos = exts.GetHeadPosition();
    while (pos) {
        if (CFileAssoc::AreRegisteredFileContextMenuEntries(exts.GetNext(pos))) {
            cnt++;
        }
    }

    reg_state_t res;
    if (cnt == 0) {
        res = NOT_REGISTERED;
    } else if (cnt == exts.GetCount()) {
        res = ALL_REGISTERED;
    } else {
        res = SOME_REGISTERED;
    }

    return res;
}

bool CFileAssoc::RegisterFolderContextMenuEntries(bool bRegister)
{
    CRegKey key;
    bool success;

    if (bRegister) {
        success = false;

        if (ERROR_SUCCESS == key.Create(HKEY_CLASSES_ROOT, _T("Directory\\shell\\") PROGID _T(".enqueue"))) {
            key.SetStringValue(NULL, ResStr(IDS_ADD_TO_PLAYLIST));

            if (ERROR_SUCCESS == key.Create(HKEY_CLASSES_ROOT, _T("Directory\\shell\\") PROGID _T(".enqueue\\command"))) {
                key.SetStringValue(NULL, strEnqueueCommand);
                success = true;
            }
        }

        if (success && ERROR_SUCCESS == key.Create(HKEY_CLASSES_ROOT, _T("Directory\\shell\\") PROGID _T(".play"))) {
            success = false;

            key.SetStringValue(NULL, ResStr(IDS_OPEN_WITH_MPC));

            if (ERROR_SUCCESS == key.Create(HKEY_CLASSES_ROOT, _T("Directory\\shell\\") PROGID _T(".play\\command"))) {
                key.SetStringValue(NULL, strOpenCommand);
                success = true;
            }
        }

    } else {
        key.Attach(HKEY_CLASSES_ROOT);
        success  = (ERROR_SUCCESS == key.RecurseDeleteKey(_T("Directory\\shell\\") PROGID _T(".enqueue")));
        success &= (ERROR_SUCCESS == key.RecurseDeleteKey(_T("Directory\\shell\\") PROGID _T(".play")));
    }

    return success;
}

bool CFileAssoc::AreRegisteredFolderContextMenuEntries()
{
    CRegKey key;
    TCHAR   buff[_MAX_PATH];
    ULONG   len = _countof(buff);
    bool    registered = false;

    if (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, _T("Directory\\shell\\") PROGID _T(".play\\command"), KEY_READ)) {
        if (ERROR_SUCCESS == key.QueryStringValue(NULL, buff, &len)) {
            registered = (strOpenCommand.CompareNoCase(CString(buff)) == 0);
        }
    }

    return registered;
}

static struct {
    CString verb;
    CString cmd;
    UINT action;
} handlers[] = {
    { _T("VideoFiles"), _T(" %1"),      IDS_AUTOPLAY_PLAYVIDEO },
    { _T("MusicFiles"), _T(" %1"),      IDS_AUTOPLAY_PLAYMUSIC },
    { _T("CDAudio"),    _T(" %1 /cd"),  IDS_AUTOPLAY_PLAYAUDIOCD },
    { _T("DVDMovie"),   _T(" %1 /dvd"), IDS_AUTOPLAY_PLAYDVDMOVIE },
};

bool CFileAssoc::RegisterAutoPlay(autoplay_t ap, bool bRegister)
{
    CString exe = GetProgramPath(true);

    int i = (int)ap;
    if (i < 0 || i >= _countof(handlers)) {
        return false;
    }

    CRegKey key;

    if (bRegister) {
        if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, _T("MediaPlayerClassic.Autorun"))) {
            return false;
        }
        key.Close();

        if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT,
                                        _T("MediaPlayerClassic.Autorun\\Shell\\Play") + handlers[i].verb + _T("\\Command"))) {
            return false;
        }
        key.SetStringValue(NULL, _T("\"") + exe + _T("\"") + handlers[i].cmd);
        key.Close();

        if (ERROR_SUCCESS != key.Create(HKEY_LOCAL_MACHINE,
                                        _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoplayHandlers\\Handlers\\MPCPlay") + handlers[i].verb + _T("OnArrival"))) {
            return false;
        }
        key.SetStringValue(_T("Action"), ResStr(handlers[i].action));
        key.SetStringValue(_T("Provider"), _T("Media Player Classic"));
        key.SetStringValue(_T("InvokeProgID"), _T("MediaPlayerClassic.Autorun"));
        key.SetStringValue(_T("InvokeVerb"), _T("Play") + handlers[i].verb);
        key.SetStringValue(_T("DefaultIcon"), exe + _T(",0"));
        key.Close();

        if (ERROR_SUCCESS != key.Create(HKEY_LOCAL_MACHINE,
                                        _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoplayHandlers\\EventHandlers\\Play") + handlers[i].verb + _T("OnArrival"))) {
            return false;
        }
        key.SetStringValue(CString("MPCPlay") + handlers[i].verb + _T("OnArrival"), _T(""));
        key.Close();
    } else {
        if (ERROR_SUCCESS != key.Create(HKEY_LOCAL_MACHINE,
                                        _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoplayHandlers\\EventHandlers\\Play") + handlers[i].verb + _T("OnArrival"))) {
            return false;
        }
        key.DeleteValue(_T("MPCPlay") + handlers[i].verb + _T("OnArrival"));
        key.Close();
    }

    return true;
}

bool CFileAssoc::IsAutoPlayRegistered(autoplay_t ap)
{
    ULONG len;
    TCHAR buff[_MAX_PATH];
    CString exe = GetProgramPath(true);

    int i = (int)ap;
    if (i < 0 || i >= _countof(handlers)) {
        return false;
    }

    CRegKey key;

    if (ERROR_SUCCESS != key.Open(HKEY_LOCAL_MACHINE,
                                  _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoplayHandlers\\EventHandlers\\Play") + handlers[i].verb + _T("OnArrival"),
                                  KEY_READ)) {
        return false;
    }
    len = _countof(buff);
    if (ERROR_SUCCESS != key.QueryStringValue(_T("MPCPlay") + handlers[i].verb + _T("OnArrival"), buff, &len)) {
        return false;
    }
    key.Close();

    if (ERROR_SUCCESS != key.Open(HKEY_CLASSES_ROOT,
                                  _T("MediaPlayerClassic.Autorun\\Shell\\Play") + handlers[i].verb + _T("\\Command"),
                                  KEY_READ)) {
        return false;
    }
    len = _countof(buff);
    if (ERROR_SUCCESS != key.QueryStringValue(NULL, buff, &len)) {
        return false;
    }
    if (_tcsnicmp(_T("\"") + exe, buff, exe.GetLength() + 1)) {
        return false;
    }
    key.Close();

    return true;
}