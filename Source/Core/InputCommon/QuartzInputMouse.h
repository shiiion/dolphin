#include <ApplicationServices/ApplicationServices.h>
#include "GenericMouse.h"
#include "ControllerInterface/Quartz/QuartzKeyboardAndMouse.h"

#ifdef __OBJC__
@class NSEvent;
#else
class NSEvent
typedef void* id;
#endif

namespace prime
{

bool InitQuartzInputMouse(DolWindowPositionObserver* window);

class QuartzInputMouse: public GenericMouse
{
public:
  explicit QuartzInputMouse(DolWindowPositionObserver* window);
  ~QuartzInputMouse();
  void InputCallback(NSEvent* event);
  void UpdateInput() override;
  void LockCursorToGameWindow() override;
  void UnlockCursor();

private:
  NSEvent*(^m_event_callback)(NSEvent*);
  std::atomic<void*> m_monitor{nullptr};
  DolWindowPositionObserver* m_window;
  std::atomic<int32_t> thread_dx;
  std::atomic<int32_t> thread_dy;
  std::mutex m_mtx;
};

}
