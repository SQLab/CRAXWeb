extern "C" {
#include "config.h"
#include "qemu-common.h"
}

#include "HostFiles.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(HostFiles, "Access to host files", "",);

void HostFiles::initialize()
{
    //m_allowWrite = s2e()->getConfig()->getBool(
    //            getConfigKey() + ".allowWrite");
    m_baseDir = s2e()->getConfig()->getString(
                getConfigKey() + ".baseDir");
    if(m_baseDir.empty())
        m_baseDir = s2e()->getOutputDirectory();

    s2e()->getCorePlugin()->onCustomInstruction.connect(
            sigc::mem_fun(*this, &HostFiles::onCustomInstruction));
}

void HostFiles::onCustomInstruction(S2EExecutionState *state, uint64_t opcode)
{
    //XXX: find a better way of allocating custom opcodes
    if (!(((opcode>>8) & 0xFF) == 0xEE)) {
        return;
    }

    opcode >>= 16;
    uint8_t op = opcode & 0xFF;
    opcode >>= 8;

    switch(op) {
    case 0: // open
        {
            uint32_t fnamePtr, flags;
            uint32_t guestFd = (uint32_t) -1;
            bool ok = true;
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EBX]), &fnamePtr, 4);
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]), &flags, 4);

            state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &guestFd, 4);

            if (!ok) {
                s2e()->getWarningsStream(state)
                    << "ERROR: symbolic argument was passed to s2e_op HostFiles "
                    << std::endl;
                break;
            }

            std::string fname;
            if(!state->readString(fnamePtr, fname)) {
                s2e()->getWarningsStream(state)
                    << "Error reading file name string from the guest" << std::endl;
                break;
            }

            unsigned i;
            for(i = 0; i < fname.size(); ++i) {
                if(!(isalnum(fname[i]) || fname[i] == ','
                        || fname[i] == '_' || fname[i] == '-')) {
                    break;
                }
            }

            if(fname.size() == 0 || i != fname.size()) {
                s2e()->getWarningsStream(state)
                    << "Guest passes ivalid file name to HostFiles plugin" << std::endl;
                break;
            }

            int fd = open((m_baseDir + "/" + fname).c_str(), O_RDONLY);
            if(fd != -1) {
                m_openFiles.push_back(fd);
                guestFd = m_openFiles.size()-1;
                state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &guestFd, 4);
            }
        }

        break;

    case 1: // close
        {
            uint32_t guestFd;
            uint32_t ret = (uint32_t) -1;

            bool ok = true;
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EBX]), &guestFd, 4);

            state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &ret, 4);

            if (!ok) {
                s2e()->getWarningsStream(state)
                    << "ERROR: symbolic argument was passed to s2e_op HostFiles "
                    << std::endl;
                break;
            }

            if(guestFd < m_openFiles.size() && m_openFiles[guestFd] != -1) {
                ret = close(m_openFiles[guestFd]);
                m_openFiles[guestFd] = -1;
                state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &ret, 4);
            }
        }

        break;

    case 2: // read
        {
            uint32_t guestFd, bufAddr, count;
            uint32_t ret = (uint32_t) -1;

            bool ok = true;
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EBX]), &guestFd, 4);
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]), &bufAddr, 4);
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EDX]), &count, 4);

            state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &ret, 4);

            if (!ok) {
                s2e()->getWarningsStream(state)
                    << "ERROR: symbolic argument was passed to s2e_op HostFiles" << std::endl;
                break;
            }

            if(count > 1024*64) {
                s2e()->getWarningsStream(state)
                    << "ERROR: count passed to HostFiles is too big" << std::endl;
                break;
            }

            if(guestFd > m_openFiles.size() || m_openFiles[guestFd] == -1) {
                break;
            }

            int fd = m_openFiles[guestFd];
            char buf[count];

            ret = read(fd, buf, count);
            if(ret == (uint32_t) -1)
                break;

            ok = state->writeMemoryConcrete(bufAddr, buf, ret);
            if(!ok) {
                s2e()->getWarningsStream(state)
                    << "ERROR: HostFiles can not write to guest buffer" << std::endl;
                break;
            }

            state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &ret, 4);
        }

        break;

    //case 3: // write
    //    break;

    default:
        s2e()->getWarningsStream(state)
                << "Invalid HostFiles opcode 0x"
                << std::hex << op << std::dec << std::endl;
        break;
    }
}


} // namespace plugins
} // namespace s2e

