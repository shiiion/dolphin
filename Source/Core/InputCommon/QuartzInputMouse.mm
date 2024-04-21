#include "QuartzInputMouse.h"
#include "Core/Host.h"
#include <Cocoa/Cocoa.h>

/**
 * This interface works by registering an event monitor and updating deltas on mouse moves.
 */

#if ! __has_feature(objc_arc)
#error "Compile this with -fobjc-arc"
#endif

int win_w = 0, win_h = 0;

namespace prime
{

bool InitQuartzInputMouse(DolWindowPositionObserver* window)
{
  g_mouse_input.reset(new QuartzInputMouse(window));
  return true;
}

QuartzInputMouse::QuartzInputMouse(DolWindowPositionObserver* window)
{
  m_window = window;
  m_event_callback = ^NSEvent*(NSEvent* event) {
    InputCallback(event);
    return event;
  };
  void (*key_callback)(void*, bool) = [](void* ctx, bool key){
    if (key)
      return;
    QuartzInputMouse* me = static_cast<QuartzInputMouse*>(ctx);
    std::lock_guard<std::mutex> lock(me->m_mtx);
    me->UnlockCursor();
  };
  [m_window addKeyWindowChangeCallback:key_callback ctx:this];
}

QuartzInputMouse::~QuartzInputMouse()
{
  [m_window removeKeyWindowChangeCallback:this];
  if (void* monitor = m_monitor.load(std::memory_order_relaxed))
    [NSEvent removeMonitor:(__bridge id)monitor];
}

void QuartzInputMouse::InputCallback(NSEvent* event)
{
  thread_dx.fetch_add([event deltaX], std::memory_order_relaxed);
  thread_dy.fetch_add([event deltaY], std::memory_order_relaxed);
}

void QuartzInputMouse::UpdateInput()
{
  this->dx += thread_dx.exchange(0, std::memory_order_relaxed);
  this->dy += thread_dy.exchange(0, std::memory_order_relaxed);
  LockCursorToGameWindow();
}

void QuartzInputMouse::LockCursorToGameWindow()
{
  bool wants_locked = Host_RendererHasFocus() && cursor_locked && [m_window isKeyWindow];
  bool is_locked = m_monitor.load(std::memory_order_relaxed);
  if (wants_locked == is_locked)
    return;
  std::lock_guard<std::mutex> lock(m_mtx);
  wants_locked = Host_RendererHasFocus() && cursor_locked && [m_window isKeyWindow];
  is_locked = m_monitor.load(std::memory_order_relaxed);
  if (wants_locked == is_locked)
    return;
  if (wants_locked)
  {
    // Disable cursor movement
    CGAssociateMouseAndMouseCursorPosition(false);
    [NSCursor hide];
    // Clear any accumulated movement
    thread_dx.store(0, std::memory_order_relaxed);
    thread_dy.store(0, std::memory_order_relaxed);
    id monitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskMouseMoved
                                                       handler:m_event_callback];
    m_monitor.store((__bridge void*)monitor, std::memory_order_relaxed);
  }
  else
  {
    UnlockCursor();
    cursor_locked = false;
  }
}

void QuartzInputMouse::UnlockCursor()
{
  void* monitor = m_monitor.load(std::memory_order_relaxed);
  if (!monitor)
    return;
  CGAssociateMouseAndMouseCursorPosition(true);
  [NSCursor unhide];
  [NSEvent removeMonitor:(__bridge id)monitor];
  m_monitor.store(nullptr, std::memory_order_relaxed);
}

}
