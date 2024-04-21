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

bool InitQuartzInputMouse();

class QuartzInputMouse: public GenericMouse
{
public:
  explicit QuartzInputMouse();
  ~QuartzInputMouse();
  void InputCallback(NSEvent* event);
  void UpdateInput() override;
  void LockCursorToGameWindow() override;

private:
  NSEvent*(^m_event_callback)(NSEvent*);
  id m_monitor = nullptr;
  std::atomic<int32_t> thread_dx;
  std::atomic<int32_t> thread_dy;
};

}
