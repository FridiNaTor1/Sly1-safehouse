#include "basic.h"
#include "gc.h"
#include <cstdint>
#include <new>

void GetBasicCid(BASIC* pbasic, CID* pcid)
{
	*pcid = pbasic->pvtbasic->cid;
}

bool FIsBasicDerivedFrom(const BASIC* basic, CID cid)
{
    if (!basic) return false;

    VTBASIC *pvt = basic->pvtbasic;

    while (pvt) {
        if (pvt->cid == cid) 
            return true;

        pvt = (VTBASIC*)pvt->pvtSuper;
    }
    return false;
}

void EnsureBasicSidebag(BASIC* pbasic)
{
    if (pbasic == nullptr)
        return;

    CSidebag* const psidebag = pbasic->psidebag.get();
    const std::uintptr_t sidebagAddress = reinterpret_cast<std::uintptr_t>(psidebag);
    // A live sidebag sits at its allocation's alignment: 16 bytes with MSVC's and libstdc++'s make_shared, but only
    // CSidebag's own alignment with libc++'s (macOS, iOS, Android), whose control block puts it 8 bytes past a
    // 16-byte boundary. Checking 16 there took every sidebag for corrupt and replaced it, losing its options.
#if defined(_LIBCPP_VERSION)
    constexpr std::uintptr_t kSidebagAlignMask = alignof(CSidebag) - 1u;
#else
    constexpr std::uintptr_t kSidebagAlignMask = 0x0fu;
#endif
    const bool fCorrupt = psidebag != nullptr &&
        (sidebagAddress < 0x10000u || (sidebagAddress & kSidebagAlignMask) != 0u);

    if (fCorrupt)
    {
        // Do not assign/reset a corrupted shared_ptr: that would dereference
        // its equally corrupted control block. Reconstruct it in place.
        std::construct_at(&pbasic->psidebag, PsidebagNew());
    }
    else if (psidebag == nullptr)
    {
        pbasic->psidebag = PsidebagNew();
    }

    if (pbasic->psidebag != nullptr)
        g_gc.AddRootSidebag(pbasic->psidebag.get());
}
