#pragma once

#include "cell.h"
#include "common.h"

#include <functional>
#include <iostream>
#include <map>
#include <unordered_map>
#include <variant>

struct PositionHasher {
    static const uint64_t N = 37;
    size_t operator() (Position pos) const;
};

class Sheet : public SheetInterface {
public:
    ~Sheet();

    void SetCell(Position pos, std::string text) override;

    Cell* CreateEmptyCell(Position pos);

    // Методы получения ячейки (для которых был вызван SetCell)
    const CellInterface* GetCell(Position pos) const override;
    CellInterface* GetCell(Position pos) override;

    void ClearCell(Position pos) override;

    Size GetPrintableSize() const override;

    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;

    // Методы получения ячейки, независимо от того, был ли вызван SetCell в этой ячейке
    const Cell* GetConcreteCell(Position pos) const;
    Cell* GetConcreteCell(Position pos);

private:
    bool HasCell(Position pos) const;

    bool HasConcreteCell(Position pos) const;

    void PrintCells(std::ostream& output, const std::function<void(const CellInterface&)>& printCell) const;

    // Ячейки
    std::unordered_map<Position, std::unique_ptr<Cell>, PositionHasher> cells_;
    // Количество элементов в строке: номер строки - количество ячеек, которые у которых выполнен SetCell
    std::map<int, int> row_to_cell_count_;
    // Количество элементов в столбце: номер столбца - количество ячеек, которые у которых выполнен SetCell
    std::map<int, int> column_to_cell_count_;
};