/* MessageResult.h
 * Message result codes for safe messaging interface
 * Used by both CTeam and CWorld messaging systems
 * For use with MechMania IV
 */

#ifndef _MESSAGE_RESULT_H_
#define _MESSAGE_RESULT_H_

// Message result codes for safe messaging interface
enum MessageResult {
  MSG_SUCCESS = 0,    // Message fully written
  MSG_TRUNCATED = 1,  // Message written but truncated to fit
  MSG_NO_SPACE = 2    // No space available (append only)
};

#endif  // ! _MESSAGE_RESULT_H_
