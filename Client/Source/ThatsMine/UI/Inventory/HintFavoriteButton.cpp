#include "HintFavoriteButton.h"

void UHintFavoriteButton::HandleClicked()
{
	on_favorite_clicked_.ExecuteIfBound(hint_index_);
}
