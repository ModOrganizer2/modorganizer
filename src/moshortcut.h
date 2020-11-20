#ifndef MODORGANIZER_MOSHORTCUT_INCLUDED
#define MODORGANIZER_MOSHORTCUT_INCLUDED

class MOShortcut
{
public:
  MOShortcut(const QString& link={});

  /// true iff intialized using a valid moshortcut link
  bool isValid() const { return m_valid; }

  bool hasInstance() const { return m_hasInstance; }

  bool hasExecutable() const { return m_hasExecutable; }

  const QString& instance() const { return m_instance; }

  const QString& executable() const { return m_executable; }

  QString toString() const;

private:
  QString m_instance;
  QString m_executable;
  bool m_valid;
  bool m_hasInstance;
  bool m_hasExecutable;
};

#endif  // MODORGANIZER_MOSHORTCUT_INCLUDED
