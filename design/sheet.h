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
    size_t operator() (Position pos) const {
        return static_cast<size_t>(pos.row * N + pos.col);
    }
};

class Sheet : public SheetInterface {
public:
    ~Sheet() {}

    void SetCell(Position pos, std::string text) override {
        // Если ячейки не существует
        if (!HasConcreteCell(pos)) {
            // Создаем ячейку
            cells_[pos] = std::make_unique<Cell>(*this);

            // Обновляем данные для вычисления размера печатной области
            ++row_to_cell_count_[pos.row];
            ++column_to_cell_count_[pos.col];
        }

        // Устанавливаем содержимое ячейки
        cells_[pos]->Set(text);
    }

    Cell* CreateEmptyCell(Position pos) {
        auto cell = GetConcreteCell(pos);
        if (!cell) {
            cells_[pos] = std::make_unique<Cell>(*this);
            cells_[pos]->Clear();
            cell = cells_[pos].get();
        }
        return cell;
    }

    // Методы получения ячейки (для которых был вызван SetCell)
    const CellInterface* GetCell(Position pos) const override {
        if (!HasCell(pos)) {
            return nullptr;
        }
        return cells_.at(pos).get();
    }
    CellInterface* GetCell(Position pos) override {
        if (!HasCell(pos)) {
            return nullptr;
        }
        return cells_[pos].get();
    }

    void ClearCell(Position pos) override {
        // Проверяем наличие ячейки (для которой был вызван SetCell)
        if (!HasCell(pos)) {
            return;
        }
        
        // Обновляем данные для вычисления размера печатной области
        --row_to_cell_count_[pos.row];
        --column_to_cell_count_[pos.col];
        if (row_to_cell_count_.at(pos.row) == 0) {
            row_to_cell_count_.erase(pos.row);
        }
        if (column_to_cell_count_.at(pos.col) == 0) {
            column_to_cell_count_.erase(pos.col);
        }

        // Превращаем ячейку в пустую ячейку
        cells_[pos]->Clear();
    }

    Size GetPrintableSize() const override {
        if (row_to_cell_count_.empty()) {
            return {};
        }

        // За счет сортировки в map-ах: 
        // последний элемент в row_to_cell_count_ - это номер самой нижней строки, в которой хранится ячейка, для которой вызван SetCell
        // последний элемент в column_to_cell_count_ - это номер самой последней колонки, в которой хранится ячейка, для которой вызван SetCell
        return {
            row_to_cell_count_.rbegin()->first + 1,
            column_to_cell_count_.rbegin()->first + 1
        };
    }

    void PrintValues(std::ostream& output) const override {
        const std::function<void(const CellInterface&)> print_cell = [&output](const CellInterface& cell){
            std::visit([&output](const auto &elem) { output << elem; }, cell.GetValue());
        };
        PrintCells(output, print_cell);
    }
    void PrintTexts(std::ostream& output) const override {
        std::function<void(const CellInterface&)> print_cell = [&output](const CellInterface& cell){
            output << cell.GetText();
        };
        PrintCells(output, print_cell);
    }

    // Методы получения ячейки, независимо от того, был ли вызван SetCell в этой ячейке
    const Cell* GetConcreteCell(Position pos) const {
        if (!HasConcreteCell(pos)) {
            return nullptr;
        }
        return cells_.at(pos).get();
    }
    Cell* GetConcreteCell(Position pos) {
        if (!HasConcreteCell(pos)) {
            return nullptr;
        }
        return cells_[pos].get();
    }

private:
    bool HasCell(Position pos) const {
        auto cell = GetConcreteCell(pos);
        return cell && !cell->IsEmpty();
    }

    bool HasConcreteCell(Position pos) const {
        if (!pos.IsValid()) {
            throw InvalidPositionException("cell check error: position is invalid"s);
        }
        return cells_.count(pos);
    }

    void PrintCells(std::ostream& output, const std::function<void(const CellInterface&)>& printCell) const {
        auto size = GetPrintableSize();
        if (size == Size{}) {
            return;
        }

        for (int i = 0; i < size.rows; ++i) {
            for (int j = 0; j < size.cols; ++j) {
                if (j != 0) {
                    output << '\t';
                }
                Position pos{i, j};
                if (!HasConcreteCell(pos)) {
                    output << ""s;
                } else {
                    printCell(*cells_.at(pos));
                }
            }
            output << '\n';
        }
    }

    // Ячейки
    std::unordered_map<Position, std::unique_ptr<Cell>, PositionHasher> cells_;
    // Количество элементов в строке: номер строки - количество ячеек, которые у которых выполнен SetCell
    std::map<int, int> row_to_cell_count_;
    // Количество элементов в столбце: номер столбца - количество ячеек, которые у которых выполнен SetCell
    std::map<int, int> column_to_cell_count_;
};