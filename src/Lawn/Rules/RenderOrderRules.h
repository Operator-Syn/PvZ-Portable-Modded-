// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <algorithm>

namespace RenderOrderRules
{
// Equal layers retain collection order, independent of addresses or payload union contents.
template<typename Iterator>
void SortByLayer(Iterator theBegin, Iterator theEnd)
{
	std::stable_sort(theBegin, theEnd, [](const auto& theFirst, const auto& theSecond)
	{
		return theFirst.mZPos < theSecond.mZPos;
	});
}
}
