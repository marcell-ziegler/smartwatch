#include "timetable.h"
#include <algorithm>

const Train *Timetable::findTrain(const std::string &number) const
{
    auto it = std::find_if(this->trains.begin(), this->trains.end(), [&](const Train &train)
                           { return train.number == number; });
    return (it != this->trains.end()) ? &*it : nullptr;
}