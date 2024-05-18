
#include <format>

#include <log.h>
#include <utility.h>
#include <windowsx.h>

#include "envshell.h"

namespace env
{

using namespace MOBase;

const int QCM_FIRST = 1;
const int QCM_LAST  = 0x7ff;

class MenuFailed : public std::runtime_error
{
public:
  MenuFailed(HRESULT r, const std::string& what)
      : runtime_error(
            std::format("{}, {}", what,
                        QString::fromStdWString(formatSystemMessage(r)).toStdString()))
  {}
};

class DummyMenu
{
public:
  DummyMenu(QString s) : m_what(s) {}

  const QString& what() const { return m_what; }

private:
  QString m_what;
};

struct IdlsFreer
{
  const std::vector<LPCITEMIDLIST>& v;

  IdlsFreer(const std::vector<LPCITEMIDLIST>& v) : v(v) {}

  ~IdlsFreer()
  {
    for (auto&& idl : v) {
      ::CoTaskMemFree(const_cast<LPITEMIDLIST>(idl));
    }
  }
};

class WndProcFilter : public QAbstractNativeEventFilter
{
public:
  using function_type =
      std::function<bool(HWND hwnd, UINT m, WPARAM wp, LPARAM lp, LRESULT* out)>;

  WndProcFilter(function_type f) : m_f(std::move(f)) {}

  bool nativeEventFilter(const QByteArray& eventType, void* message,
                         qintptr* result) override
  {
    MSG* msg = (MSG*)message;
    if (!msg) {
      return false;
    }

    LRESULT lr = 0;

    const bool r = m_f(msg->hwnd, msg->message, msg->wParam, msg->lParam, &lr);

    if (result) {
      *result = lr;
    }

    return r;
  }

private:
  function_type m_f;
};

HWND getHWND(QMainWindow* mw)
{
  if (mw) {
    return (HWND)mw->winId();
  } else {
    return 0;
  }
}

// adapted from
// https://devblogs.microsoft.com/oldnewthing/20040928-00/?p=37723
//
HRESULT IContextMenu_GetCommandString(IContextMenu* pcm, UINT_PTR idCmd, UINT uFlags,
                                      UINT* pwReserved, LPWSTR pszName, UINT cchMax)
{
  // Callers are expected to be using Unicode.
  if (!(uFlags & GCS_UNICODE)) {
    return E_INVALIDARG;
  }

  // Some context menu handlers have off-by-one bugs and will
  // overflow the output buffer. Let�s artificially reduce the
  // buffer size so a one-character overflow won�t corrupt memory.
  if (cchMax <= 1) {
    return E_FAIL;
  }

  cchMax--;

  // First try the Unicode message.  Preset the output buffer
  // with a known value because some handlers return S_OK without
  // doing anything.
  pszName[0] = L'\0';

  HRESULT hr = pcm->GetCommandString(idCmd, uFlags, pwReserved, (LPSTR)pszName, cchMax);

  if (SUCCEEDED(hr) && pszName[0] == L'\0') {
    // Rats, a buggy IContextMenu handler that returned success
    // even though it failed.
    hr = E_NOTIMPL;
  }

  if (FAILED(hr)) {
    // try again with ANSI � pad the buffer with one extra character
    // to compensate for context menu handlers that overflow by
    // one character.
    LPSTR pszAnsi = (LPSTR)LocalAlloc(LMEM_FIXED, (cchMax + 1) * sizeof(CHAR));

    if (pszAnsi) {
      pszAnsi[0] = '\0';

      hr = pcm->GetCommandString(idCmd, uFlags & ~GCS_UNICODE, pwReserved, pszAnsi,
                                 cchMax);

      if (SUCCEEDED(hr) && pszAnsi[0] == '\0') {
        // Rats, a buggy IContextMenu handler that returned success
        // even though it failed.
        hr = E_NOTIMPL;
      }

      if (SUCCEEDED(hr)) {
        if (MultiByteToWideChar(CP_ACP, 0, pszAnsi, -1, pszName, cchMax) == 0) {
          hr = E_FAIL;
        }
      }

      LocalFree(pszAnsi);

    } else {
      hr = E_OUTOFMEMORY;
    }
  }

  return hr;
}

ShellMenu::ShellMenu(QMainWindow* mw) : m_mw(mw) {}

void ShellMenu::addFile(QFileInfo fi)
{
  m_files.emplace_back(std::move(fi));
}

int ShellMenu::fileCount() const
{
  return static_cast<int>(m_files.size());
}

void ShellMenu::exec(const QPoint& pos)
{
  HMENU menu = getMenu();
  if (!menu) {
    return;
  }

  try {
    const auto hwnd = getHWND(m_mw);

    auto filter = std::make_unique<WndProcFilter>(
        [&](HWND h, UINT m, WPARAM wp, LPARAM lp, LRESULT* out) {
          return wndProc(h, m, wp, lp, out);
        });

    QCoreApplication::instance()->installNativeEventFilter(filter.get());

    const int cmd =
        TrackPopupMenuEx(menu, TPM_RETURNCMD, pos.x(), pos.y(), hwnd, nullptr);

    if (m_mw) {
      if (auto* sb = m_mw->statusBar()) {
        sb->clearMessage();
      }
    }

    if (cmd <= 0) {
      return;
    }

    invoke(pos, cmd - QCM_FIRST);
  } catch (MenuFailed& e) {
    if (m_files.size() == 1) {
      log::error("can't exec shell menu for '{}': {}",
                 QDir::toNativeSeparators(m_files[0].absoluteFilePath()), e.what());
    } else {
      log::error("can't exec shell menu for {} files: {}", m_files.size(), e.what());
    }
  }
}

HMENU ShellMenu::getMenu()
{
  if (!m_menu) {
    create();
  }

  return m_menu.get();
}

bool ShellMenu::wndProc(HWND h, UINT m, WPARAM wp, LPARAM lp, LRESULT* out)
{
  if (m == WM_MENUSELECT) {
    HANDLE_WM_MENUSELECT(h, wp, lp, onMenuSelect);
    return true;
  }

  if (m_cm3) {
    const auto r = m_cm3->HandleMenuMsg2(m, wp, lp, out);

    if (SUCCEEDED(r)) {
      return true;
    }
  }

  if (m_cm2) {
    const auto r = m_cm2->HandleMenuMsg(m, wp, lp);

    if (SUCCEEDED(r)) {
      if (out) {
        *out = 0;
      }

      return true;
    }
  }

  return false;
}

// adapted from
// https://devblogs.microsoft.com/oldnewthing/20040928-00/?p=37723
//
void ShellMenu::onMenuSelect(HWND hwnd, HMENU hmenu, int item, HMENU hmenuPopup,
                             UINT flags)
{
  if (m_cm && item >= QCM_FIRST && item <= QCM_LAST) {
    WCHAR szBuf[MAX_PATH];

    const auto r = IContextMenu_GetCommandString(m_cm.get(), item - QCM_FIRST,
                                                 GCS_HELPTEXTW, NULL, szBuf, MAX_PATH);

    if (FAILED(r)) {
      lstrcpynW(szBuf, L"No help available.", MAX_PATH);
    }

    if (m_mw) {
      if (auto* sb = m_mw->statusBar()) {
        sb->showMessage(QString::fromWCharArray(szBuf));
      }
    }
  }
}

void ShellMenu::create()
{
  if (m_files.empty()) {
    log::warn("showShellMenu(): no files given");
    return;
  }

  try {
    auto idls = createIdls(m_files);

    if (idls.empty()) {
      log::error("no idls, can't create context menu");
      return;
    }

    IdlsFreer freer(idls);

    auto array = createItemArray(idls);

    createContextMenu(array.get());
    createPopupMenu(m_cm.get());
  } catch (DummyMenu& dm) {
    m_menu = createDummyMenu(dm.what());
  } catch (MenuFailed& e) {
    if (m_files.size() == 1) {
      log::error("can't create shell menu for '{}': {}",
                 QDir::toNativeSeparators(m_files[0].absoluteFilePath()), e.what());
    } else {
      log::error("can't create shell menu for {} files: {}", m_files.size(), e.what());
    }

    m_menu = createDummyMenu(QObject::tr("No menu available"));
  }
}

HMenuPtr ShellMenu::createDummyMenu(const QString& what)
{
  try {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
      const auto e = GetLastError();
      throw MenuFailed(e, "CreatePopupMenu failed");
    }

    if (!AppendMenuW(menu, MF_STRING | MF_DISABLED, 0, what.toStdWString().c_str())) {
      const auto e = GetLastError();
      throw MenuFailed(e, "AppendMenuW failed");
    }

    return HMenuPtr(menu);
  } catch (MenuFailed& e) {
    log::error("{}", what);
    log::error("additionally, creating the dummy menu failed: {}", e.what());

    return {};
  }
}

std::vector<LPCITEMIDLIST> ShellMenu::createIdls(const std::vector<QFileInfo>& files)
{
  std::vector<LPCITEMIDLIST> idls;
  std::optional<QDir> parent;

  for (auto&& f : files) {
    const auto path = QDir::toNativeSeparators(f.absoluteFilePath()).toStdWString();

    if (!parent) {
      parent = f.absoluteDir();
    } else {
      if (*parent != f.absoluteDir()) {
        throw DummyMenu(QObject::tr("Selected files must be in the same directory"));
      }
    }

    auto item    = createShellItem(path);
    auto pidlist = getPersistIDList(item.get());
    auto absIdl  = getIDList(pidlist.get());

    idls.push_back(absIdl.release());
  }

  return idls;
}

COMPtr<IShellItemArray> ShellMenu::createItemArray(std::vector<LPCITEMIDLIST>& idls)
{
  IShellItemArray* array = nullptr;
  auto r = SHCreateShellItemArrayFromIDLists(static_cast<UINT>(idls.size()), &idls[0],
                                             &array);

  if (FAILED(r)) {
    throw MenuFailed(r, "SHCreateShellItemArrayFromIDLists failed");
  }

  return COMPtr<IShellItemArray>(array);
}

void ShellMenu::createContextMenu(IShellItemArray* array)
{
  IContextMenu* cm = nullptr;

  auto r =
      array->BindToHandler(nullptr, BHID_SFUIObject, IID_IContextMenu, (void**)&cm);

  if (FAILED(r)) {
    throw MenuFailed(r, "BindToHandler failed");
  }

  m_cm.reset(cm);

  {
    IContextMenu2* cm2 = nullptr;
    if (SUCCEEDED(m_cm->QueryInterface(IID_IContextMenu2, (void**)&cm2))) {
      m_cm2.reset(cm2);
    }
  }

  {
    IContextMenu3* cm3 = nullptr;
    if (SUCCEEDED(m_cm->QueryInterface(IID_IContextMenu3, (void**)&cm3))) {
      m_cm3.reset(cm3);
    }
  }
}

void ShellMenu::createPopupMenu(IContextMenu* cm)
{
  HMENU hmenu = CreatePopupMenu();
  if (!hmenu) {
    const auto e = GetLastError();
    throw MenuFailed(e, "CreatePopupMenu failed");
  }

  const auto r = cm->QueryContextMenu(hmenu, 0, QCM_FIRST, QCM_LAST, CMF_EXTENDEDVERBS);

  if (FAILED(r)) {
    throw MenuFailed(r, "QueryContextMenu failed");
  }

  m_menu.reset(hmenu);
}

COMPtr<IShellItem> ShellMenu::createShellItem(const std::wstring& path)
{
  IShellItem* item = nullptr;

  auto r =
      SHCreateItemFromParsingName(path.c_str(), nullptr, IID_IShellItem, (void**)&item);

  if (FAILED(r)) {
    throw MenuFailed(r, "SHCreateItemFromParsingName failed");
  }

  return COMPtr<IShellItem>(item);
}

COMPtr<IPersistIDList> ShellMenu::getPersistIDList(IShellItem* item)
{
  IPersistIDList* idl = nullptr;
  auto r              = item->QueryInterface(IID_IPersistIDList, (void**)&idl);

  if (FAILED(r)) {
    throw MenuFailed(r, "QueryInterface IID_IPersistIDList failed");
  }

  return COMPtr<IPersistIDList>(idl);
}

CoTaskMemPtr<LPITEMIDLIST> ShellMenu::getIDList(IPersistIDList* pidlist)
{
  LPITEMIDLIST absIdl = nullptr;
  auto r              = pidlist->GetIDList(&absIdl);

  if (FAILED(r)) {
    throw MenuFailed(r, "GetIDList failed");
  }

  return CoTaskMemPtr<LPITEMIDLIST>(absIdl);
}

void ShellMenu::invoke(const QPoint& p, int cmd)
{
  const auto hwnd = getHWND(m_mw);

  CMINVOKECOMMANDINFOEX info = {};

  info.cbSize   = sizeof(info);
  info.fMask    = CMIC_MASK_UNICODE | CMIC_MASK_PTINVOKE;
  info.hwnd     = hwnd;
  info.lpVerb   = MAKEINTRESOURCEA(cmd);
  info.lpVerbW  = MAKEINTRESOURCEW(cmd);
  info.nShow    = SW_SHOWNORMAL;
  info.ptInvoke = {p.x(), p.y()};

  // note: this calls the query version because the Qt even loop hasn't run
  // yet and shift is still considered pressed
  const auto m = QApplication::queryKeyboardModifiers();

  if (m & Qt::ShiftModifier) {
    info.fMask |= CMIC_MASK_SHIFT_DOWN;
  }

  if (m & Qt::ControlModifier) {
    info.fMask |= CMIC_MASK_CONTROL_DOWN;
  }

  const auto r = m_cm->InvokeCommand((CMINVOKECOMMANDINFO*)&info);

  if (FAILED(r)) {
    throw MenuFailed(r, std::format("InvokeCommand failed, verb={}", cmd));
  }
}

ShellMenuCollection::ShellMenuCollection(QMainWindow* mw) : m_mw(mw), m_active(nullptr)
{}

void ShellMenuCollection::addDetails(QString s)
{
  m_details.emplace_back(std::move(s));
}

void ShellMenuCollection::add(QString name, ShellMenu m)
{
  m_menus.push_back({name, std::move(m)});
}

void ShellMenuCollection::exec(const QPoint& pos)
{
  HMENU menu = ::CreatePopupMenu();
  if (!menu) {
    const auto e = GetLastError();

    log::error("CreatePopupMenu for merged menus failed, {}", formatSystemMessage(e));

    return;
  }

  if (!m_details.empty()) {
    for (auto&& d : m_details) {
      const auto s = d.toStdWString();
      const auto r = AppendMenuW(menu, MF_STRING | MF_DISABLED, 0, s.c_str());

      if (!r) {
        const auto e = GetLastError();
        log::error("AppendMenuW failed for details '{}', {}", d,
                   formatSystemMessage(e));
      }
    }

    const auto r = AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    if (!r) {
      const auto e = GetLastError();
      log::error("AppendMenuW failed for separator, {}", formatSystemMessage(e));
    }
  }

  for (auto&& m : m_menus) {
    auto hmenu = m.menu.getMenu();
    if (!hmenu) {
      continue;
    }

    const auto r =
        AppendMenuW(menu, MF_POPUP | MF_STRING, reinterpret_cast<UINT_PTR>(hmenu),
                    m.name.toStdWString().c_str());

    if (!r) {
      const auto e = GetLastError();

      log::error("AppendMenuW failed for merged menu {}, {}", m.name,
                 formatSystemMessage(e));

      continue;
    }
  }

  auto hwnd = getHWND(m_mw);

  auto filter = std::make_unique<WndProcFilter>(
      [&](HWND h, UINT m, WPARAM wp, LPARAM lp, LRESULT* out) {
        return wndProc(h, m, wp, lp, out);
      });

  QCoreApplication::instance()->installNativeEventFilter(filter.get());

  const int cmd =
      TrackPopupMenuEx(menu, TPM_RETURNCMD, pos.x(), pos.y(), hwnd, nullptr);

  if (m_mw) {
    if (auto* sb = m_mw->statusBar()) {
      sb->clearMessage();
    }
  }

  if (cmd <= 0) {
    return;
  }

  if (!m_active) {
    log::debug("SMC: command {} selected without active submenu", cmd);
    return;
  }

  const auto realCmd = cmd - QCM_FIRST;

  log::debug("SMC: invoking {} on {}", realCmd, m_active->name);
  m_active->menu.invoke(pos, realCmd);
}

bool ShellMenuCollection::wndProc(HWND h, UINT m, WPARAM wp, LPARAM lp, LRESULT* out)
{
  if (m == WM_MENUSELECT) {
    auto* oldActive = m_active;
    m_active        = nullptr;

    HANDLE_WM_MENUSELECT(h, wp, lp, onMenuSelect);

    if (!m_active && oldActive) {
      // this was not a top level, forward to active
      m_active = oldActive;
    } else if (m_active && m_active == oldActive) {
      // same top level menu was selected twice, ignore
      return true;
    } else if (m_active && m_active != oldActive) {
      // new top level selected
      log::debug("SMC: switching to {}", m_active->name);
    }
  }

  if (!m_active) {
    // no active menu, forward it to the default handler
    return false;
  }

  return m_active->menu.wndProc(h, m, wp, lp, out);
}

void ShellMenuCollection::onMenuSelect(HWND hwnd, HMENU hmenu, int item,
                                       HMENU hmenuPopup, UINT flags)
{
  for (auto&& m : m_menus) {
    if (m.menu.getMenu() == hmenuPopup) {
      m_active = &m;
      break;
    }
  }
}

}  // namespace env
