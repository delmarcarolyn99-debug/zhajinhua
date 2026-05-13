#include "card.h"

std::string Card::toString() const {
    std::string s;
    switch (suit) {
        //利用分支循环语句依次创建牌的花色
    case Suit::SPADE:   s = "黑桃"; break;
    case Suit::HEART:   s = "红桃"; break;
    case Suit::CLUB:    s = "梅花"; break;
    case Suit::DIAMOND: s = "方块"; break;
    }
    //定义牌大小A>K>Q>……>3>2
    if (value <= 10) s += std::to_string(value);
    else if (value == 11) s += "J";
    else if (value == 12) s += "Q";
    else if (value == 13) s += "K";
    else if (value == 14) s += "A";

    return s;
}