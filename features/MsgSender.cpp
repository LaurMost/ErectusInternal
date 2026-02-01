#include "MsgSender.h"

#include <memory>

#include "../ErectusMemory.h"
#include "../ErectusProcess.h"
#include "../settings.h"

bool MsgSender::IsEnabled()
{
	if (!Settings::msgWriter.enabled)
		return false;

	return Patcher(Settings::msgWriter.enabled);
}

bool MsgSender::Send(void* message, const size_t size)
{
	if (!IsEnabled())
		return false;

	const auto allocSize = size + sizeof(ExternalFunction);
	const auto allocAddress = ErectusProcess::AllocEx(allocSize);
	if (allocAddress == 0)
		return false;

	ExternalFunction externalFunctionData = {
		.address = ErectusProcess::exe + OFFSET_MESSAGE_SENDER,
		.rcx = allocAddress + sizeof(ExternalFunction),
		.rdx = 0,
		.r8 = 0,
		.r9 = 0
	};

	const auto pageData = std::make_unique<BYTE[]>(allocSize);
	memset(pageData.get(), 0x00, allocSize);
	memcpy(pageData.get(), &externalFunctionData, sizeof externalFunctionData);
	memcpy(&pageData.get()[sizeof(ExternalFunction)], message, size);
	const auto written = ErectusProcess::Wpm(allocAddress, pageData.get(), allocSize);

	if (!written)
	{
		ErectusProcess::FreeEx(allocAddress);
		return false;
	}

	const auto paramAddress = allocAddress + sizeof ExternalFunction::ASM;
	auto* const thread = CreateRemoteThread(ErectusProcess::handle, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(allocAddress),
		reinterpret_cast<LPVOID>(paramAddress), 0, nullptr);

	if (thread == nullptr)
	{
		ErectusProcess::FreeEx(allocAddress);
		return false;
	}

	const auto threadResult = WaitForSingleObject(thread, 3000);
	CloseHandle(thread);

	if (threadResult == WAIT_TIMEOUT)
		return false;

	ErectusProcess::FreeEx(allocAddress);

	return true;
}

bool MsgSender::Patcher(const bool enabled)
{
    // Patch 1: Message Patcher (MP)
    BYTE mpCheck[2];
    if (!ErectusProcess::Rpm(ErectusProcess::exe + OFFSET_FAKE_MESSAGE, &mpCheck, sizeof mpCheck))
        return false;

    BYTE mpEnabled[] = { 0xEB, 0x00 };
    BYTE mpDisabled[] = { 0xEB, 0x02 };

    if (enabled)
    {
        if (memcmp(mpCheck, mpEnabled, sizeof mpEnabled) != 0)
        {
            if (!ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_FAKE_MESSAGE, &mpEnabled, sizeof mpEnabled))
                return false;
        }
    }
    else
    {
        if (memcmp(mpCheck, mpDisabled, sizeof mpDisabled) != 0)
        {
            if (!ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_FAKE_MESSAGE, &mpDisabled, sizeof mpDisabled))
                return false;
        }
    }

    // Patch 2: Extended Patcher (EP)
    BYTE epCheck[2];
    if (!ErectusProcess::Rpm(ErectusProcess::exe + OFFSET_FAKE_MESSAGE_EX, &epCheck, sizeof epCheck))
        return false;

    BYTE epEnabled[] = { 0xEB, 0x02 };
    BYTE epDisabled[] = { 0x76, 0x04 };

    if (enabled)
    {
        if (memcmp(epCheck, epEnabled, sizeof epEnabled) != 0)
        {
            if (!ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_FAKE_MESSAGE_EX, &epEnabled, sizeof epEnabled))
                return false;
        }
    }
    else
    {
        if (memcmp(epCheck, epDisabled, sizeof epDisabled) != 0)
        {
            if (!ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_FAKE_MESSAGE_EX, &epDisabled, sizeof epDisabled))
                return false;
        }
    }

    return true;
}
