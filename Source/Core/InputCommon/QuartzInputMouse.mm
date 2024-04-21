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

bool InitQuartzInputMouse()
{
  g_mouse_input = new QuartzInputMouse();
  return true;
}

QuartzInputMouse::QuartzInputMouse()
{
  m_event_callback = ^NSEvent*(NSEvent* event) {
    InputCallback(event);
    return event;
  };
}

QuartzInputMouse::~QuartzInputMouse()
{
  if (m_monitor)
    [NSEvent removeMonitor:m_monitor];
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
  bool wants_locked = Host_RendererHasFocus() && cursor_locked;
  bool is_locked = m_monitor;
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
    m_monitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskMouseMoved
                                                      handler:m_event_callback];
  }
  else
  {
    CGAssociateMouseAndMouseCursorPosition(true);
    [NSCursor unhide];
    [NSEvent removeMonitor:m_monitor];
    m_monitor = nullptr;
    cursor_locked = false;
  }
}

}
