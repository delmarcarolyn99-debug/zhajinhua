#pragma once
#include <string>
// 花色：方块=1, 梅花=2, 红桃=3, 黑桃=4
enum class Suit {
    DIAMOND = 1,
    CLUB=2,
    HEART=3,
    SPADE=4
};

struct Card {
    Suit suit;
    int value; // 2-10, 11(J), 12(Q), 13(K), 14(A)
    Card(Suit s, int v) : suit(s), value(v) {}

    // 获取牌的文字描述，比如 "黑桃A"
    std::string toString() const;

    int getValue() const {
        return value * 10 + (int)suit;
    }
};