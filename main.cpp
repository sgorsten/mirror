#include "refl.h"
#include "graph.h"

#include <iostream>

class Gold
{
    int gold;
public:
    Gold(int gold) : gold(gold) {}
    Gold(Gold && r) : gold(r.gold) { r.gold = 0; }
    int GetAmount() const { return gold; }
};

class Character
{
    int     hitPoints;
    int     attackDamage;
    float   attackTime;
    float   cooldown;
    int     gold;
public:
    Character(int hitPoints, int attackDamage, float attackTime) : hitPoints(hitPoints), attackDamage(attackDamage), attackTime(attackTime), cooldown(), gold(50) {}

    bool IsAlive() const { return hitPoints > 0; }
    float GetDamagePerSecond() const { return attackDamage / attackTime; }
    void AttackCharacter(Character & target) { target.TakeDamage(attackDamage); cooldown += attackTime; }
    void TakeDamage(int damage) { hitPoints -= damage; }

    Gold DropGold() { int g = gold; gold = 0; return Gold(g); }
    void GiveGold(Gold g) { gold += g.GetAmount(); }
    int GetGold() const { return gold; }
};

template<class T> void Print(const T & value) { std::cout << value << std::endl; }

int main()
{
    TypeLibrary types;

    Character player(100, 20, 0.5f), enemy(30, 10, 1.0f);

    auto prog = std::make_shared<const Program>(2, std::vector<Line>{
        {types.BindFunction(&Character::GetDamagePerSecond,"getDps"), {-1}},
        {types.BindFunction(&Print<float>,"print"), {0}},
        {types.BindFunction(&Character::AttackCharacter,"attack"), {-1, -2}},
        {types.BindFunction(&Character::AttackCharacter,"attack"), {-1, -2}},
        {types.BindFunction(&Character::IsAlive,"isAlive"), {-2}},
        {types.BindFunction(&Print<bool>,"print"), {4}},
        {types.BindFunction(&Character::DropGold,"dropGold"), {-2}},
        {types.BindFunction(&Character::GiveGold,"giveGold"), {-1,6}},
        {types.BindFunction(&Character::GetGold,"getGold"), {-1}},
        {types.BindFunction(&Print<int>,"print"), {8}},
        {types.BindFunction(&Character::GiveGold,"giveGold"), {-1,6}}, // Gold was already passed by move to this function, so it will be empty
        {types.BindFunction(&Character::GetGold,"getGold"), {-1}},
        {types.BindFunction(&Print<int>,"print"), {11}}, // Should see same result (100) as above
    });
    std::cout << *prog << std::endl;
    
    const auto ev = Event<Character &, Character &>(prog);

    ev(player,enemy);

    return 0;
}