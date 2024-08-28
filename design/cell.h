#pragma once

#include "common.h"
#include "formula.h"
#include "sheet.h"

#include <algorithm>
#include <optional>
#include <unordered_set>

using namespace std::literals;

class Cell : public CellInterface {
public:
    explicit Cell(Sheet& sheet);
    ~Cell();

    void Set(std::string text);

    void Clear();

    Value GetValue() const override;

    std::string GetText() const override;
    
    std::vector<Position> GetReferencedCells() const override;

    bool IsEmpty() const;

private:
    class Impl {
        public:
            Impl() = default;
            virtual ~Impl() = default;

            virtual Value GetValue() const = 0;
            virtual std::string GetText() const = 0;
            virtual std::string GetInitialText() const = 0;
    };
    class EmptyImpl final : public Impl {
        public:
            Value GetValue() const override;
            std::string GetText() const override;
            std::string GetInitialText() const override;
    };
    class TextImpl final : public Impl {
        public:
            TextImpl(std::string text);
            Value GetValue() const override;
            std::string GetText() const override;
            std::string GetInitialText() const override;
        private: 
            std::string text_;
    };
    class FormulaImpl final : public Impl {
        public:
            FormulaImpl(std::string text, std::unique_ptr<FormulaInterface> formula, Sheet& sheet);
            Value GetValue() const override;
            std::string GetText() const override;
            std::string GetInitialText() const override;
            std::vector<Position> GetReferencedCells() const;

        private: 
            std::string text_;
            std::unique_ptr<FormulaInterface> formula_;
            // таблица ячейки 
            // (необходима для получения доступа к ячейкам в случае формульных ячеек, содержащих в формулах индексы на ячейки)
            Sheet& sheet_;
    };

private:
    void ClearValueCache();

    bool CheckCircularDependency(const std::vector<Position>& referenced_cells) const;

    bool CheckCircularDependency(Position cell_position, std::vector<Position>& checked_cells) const;

private:
    std::unique_ptr<Impl> impl_;
    // таблица ячейки
    Sheet& sheet_;

    // ячейки, которые ссылаются на текущую ячейку (т.е. ячейки, чье вычисление значения зависит от текущей ячейки)
    // (необходим для инвалидации кэша)
    std::unordered_set<Cell*> cells_from_; 

    // Кэш вычисленного значения
    mutable std::optional<Value> value_cache_;
};