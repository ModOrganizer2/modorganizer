#ifndef ILOCKEDWAITINGFORPROCESS_H
#define ILOCKEDWAITINGFORPROCESS_H

class QString;

class ILockedWaitingForProcess
{
public:
  virtual bool unlockForced() const = 0;
  virtual void setProcessName(QString const &) = 0;
};

#endif // ILOCKEDWAITINGFORPROCESS_H
