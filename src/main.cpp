#include "refl.h"
#include "graph.h"

#include <GL/freeglut.h>

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

void OnIdle() 
{ 
    glutPostRedisplay(); 
}

void RenderText(int x, int y, const std::string & text)
{
    glRasterPos2i(x,y);
    for(auto ch : text) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, ch);
}

void OnDisplay()
{  
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glOrtho(0, 1280, 720, 0, -1, 1);

    RenderText(640, 360, "This is a test");

    glPopMatrix();   
    glPopAttrib();

    glutSwapBuffers();
}

int main(int argc, char * argv[])
{
    TypeLibrary types;
    types.BindFunction(&Print<int>,"printi");
    types.BindFunction(&Print<bool>,"printb");
    types.BindFunction(&Print<float>,"printf");
    types.BindFunction(&Character::IsAlive,"isAlive");
    types.BindFunction(&Character::GetDamagePerSecond,"getDps");
    types.BindFunction(&Character::GetGold,"getGold");
    types.BindFunction(&Character::AttackCharacter,"attack");
    types.BindFunction(&Character::DropGold,"dropGold");
    types.BindFunction(&Character::GiveGold,"giveGold");    



    Character player(100, 20, 0.5f), enemy(30, 10, 1.0f);
  
    auto prog = std::make_shared<const Program>(2, std::vector<Line>{
        {types.GetFunction("getDps"), {-1}},
        {types.GetFunction("printf"), {0}},
        {types.GetFunction("attack"), {-1, -2}},
        {types.GetFunction("attack"), {-1, -2}},
        {types.GetFunction("isAlive"), {-2}},
        {types.GetFunction("printb"), {4}},
        {types.GetFunction("dropGold"), {-2}},
        {types.GetFunction("giveGold"), {-1,6}},
        {types.GetFunction("getGold"), {-1}},
        {types.GetFunction("printi"), {8}},
        {types.GetFunction("giveGold"), {-1,6}}, // Gold was already passed by move to this function, so it will be empty
        {types.GetFunction("getGold"), {-1}},
        {types.GetFunction("printi"), {11}}, // Should see same result (100) as above
    });
    std::cout << *prog << std::endl;
    
    const auto ev = Event<Character &, Character &>(prog);

    ev(player,enemy);

    glutInit(&argc, argv);
    glutInitWindowSize(1280, 720);
    glutInitDisplayMode(GLUT_RGBA|GLUT_DOUBLE|GLUT_DEPTH);
    glutCreateWindow("libmirror call graph visualizer");

    glutIdleFunc(OnIdle);
    glutDisplayFunc(OnDisplay);
    //glutReshapeFunc(OnReshape);
    //glutMotionFunc(OnMotion);
    //glutMouseFunc(OnMouse);
    //glutKeyboardFunc(OnKeyboard);
    glutMainLoop();

    return 0;
}