#ifndef GRAPH_H
#define GRAPH_H

#include "refl.h"

struct Line
{
    Function func;
    std::vector<int> args;
};
std::ostream & operator << (std::ostream & out, const Line & line);

class Program
{
    size_t argCount;
    std::vector<Line> lines;
public:
    Program(size_t argCount, std::vector<Line> _lines) : argCount(argCount), lines(move(_lines))
    {
        for(size_t i=0; i<lines.size(); ++i)
        {
            assert(lines[i].args.size() <= 4);
            for(int arg : lines[i].args)
            {
                assert(arg >= -(int)argCount && arg < (int)i);
            }
        }
    }

    size_t GetArgumentCount() const { return argCount; }

    void Execute(const std::vector<void *> & args) const
    {
        std::vector<std::shared_ptr<void>> results;
        for(auto & line : lines)
        {
            void * callArgs[4];
            for(size_t i=0; i<line.args.size(); ++i)
            {
                callArgs[i] = line.args[i] < 0 ? args[-line.args[i]-1] : results[line.args[i]].get();
            }
            results.push_back(line.func.Invoke(callArgs));
        }
    }

    friend std::ostream & operator << (std::ostream & out, const Program & prog);
};

template<class... P> class Event
{
    std::shared_ptr<const Program> program;
public:
    Event(std::shared_ptr<const Program> program) : program(move(program)) { assert(this->program->GetArgumentCount() == sizeof...(P)); }

    void operator() (P... p) const { program->Execute({&p...}); }
};

#endif