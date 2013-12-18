#ifndef OPCODES_H_
# define OPCODES_H_

#include "SharedDefines.h"
#include "Session.h"
#include "SessionSocket.h"
#include <map>

enum Opcodes
{
    SMSG_WELCOME        = 1,
    CMSG_AUTH           = 2,
    SMSG_AUTH_RESULT    = 3,
    CMSG_SIP            = 4,
    SMSG_SIP            = 5,
    SMSG_CONTACT_LOGIN  = 6,
    SMSG_CONTACT_LOGOUT = 7,
    CMSG_CHAT_TEXT      = 8,
    SMSG_CHAT_TEXT      = 9,
    MSG_MAX                ,
};

class OpcodeMgr
{
public:
    struct OpcodeDefinition
    {
        uint16 opcode;
        void (Session::*func)(Packet&);
    };

    static OpcodeDefinition const* getOpcodeDefinition(uint16 opcode);

};

#endif /* !OPCODES_H_ */
