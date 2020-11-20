#ifndef STACKDATA_H
#define STACKDATA_H

namespace MOShared {


class StackData {
  friend bool operator==(const StackData &LHS, const StackData &RHS);
  friend bool operator<(const StackData &LHS, const StackData &RHS);
public:

  StackData();
  StackData(const char *function, int line);

  std::string toString() const;

private:

  void load_modules(HANDLE process, DWORD processID);

  void initTrace();

private:

  static const int FRAMES_TO_SKIP = 1;
  static const int FRAMES_TO_CAPTURE = 20;

private:

  LPVOID m_Stack[FRAMES_TO_CAPTURE];
  USHORT m_Count;
  std::string m_Function;
  int m_Line;

};


bool operator==(const StackData &LHS, const StackData &RHS);

bool operator<(const StackData &LHS, const StackData &RHS);

} // namespace MOShared


#endif // STACKDATA_H
