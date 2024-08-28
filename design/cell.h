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
    explicit Cell(Sheet& sheet) :
        sheet_(sheet) 
    {}
    ~Cell() {}

    void Set(std::string text) {
        // Если происходит установка такого же текста: выход
        if (impl_ && impl_->GetInitialText() == text) {
            return;
        }

        // Признак, что текст ячейки - формула
        bool is_formula = (!text.empty() && text.at(0) == '=' && text.size() > 1);
        std::unique_ptr<FormulaInterface> formula;
        if (is_formula) {
            // Парсим формулу
            formula = ParseFormula(std::string(text.begin() + 1, text.end()));
            // Выполняем проверку на наличие циклической зависимости в новой формуле
            if (CheckCircularDependency(formula->GetReferencedCells())) {
                throw CircularDependencyException("Invalid formula: found circular dependency"s);
            }
        }

        // Сбрасываем кэш (рекурсивно)
        ClearValueCache();

        // У ячеек, от которых зависело значение тек. ячейки: убираем связь
        for (auto referenced_cell : GetReferencedCells()) {
            if (referenced_cell.IsValid()) {
                sheet_.GetConcreteCell(referenced_cell)->cells_from_.erase(this);    
            }            
        }

        // Определяем новую реализацию
        if (is_formula) {
            impl_ = std::make_unique<FormulaImpl>(text, std::move(formula), sheet_);

            // У ячеек, от которых зависит значение тек. ячейки: устанавливаем связь к тек. ячейке
            for (auto referenced_cell : GetReferencedCells()) {
                if (!referenced_cell.IsValid()) {
                    continue;
                }
                // Если ячейки из формулы не существует: создаем пустую ячейку
                auto cell = sheet_.GetConcreteCell(referenced_cell);
                if (!cell) {
                    cell = sheet_.CreateEmptyCell(referenced_cell);
                }
                cell->cells_from_.insert(this);
            }
        } else {
            impl_ = std::make_unique<TextImpl>(std::move(text));
        }
    }

    void Clear() {
        // Сбрасываем кэш (рекурсивно)
        ClearValueCache();

        impl_ = std::make_unique<EmptyImpl>();
    }

    Value GetValue() const override {
        if (!value_cache_.has_value()) {
            value_cache_ = impl_->GetValue();
        }
        return *value_cache_;
    }

    std::string GetText() const override {
        return impl_->GetText();
    }
    
    std::vector<Position> GetReferencedCells() const override {
        if (!impl_) {
            return {};
        }
        auto formula_impl = dynamic_cast<FormulaImpl*>(impl_.get());
        if (!formula_impl) {
            return {};
        }
        return formula_impl->GetReferencedCells();
    }

    bool IsEmpty() const {
        return !impl_ || dynamic_cast<EmptyImpl*>(impl_.get());
    }

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
            Value GetValue() const override { return ""s; }
            std::string GetText() const override { return ""s; }
            std::string GetInitialText() const override { return ""s; }
    };
    class TextImpl final : public Impl {
        public:
            TextImpl(std::string text) :
                text_(text) 
            {}
            Value GetValue() const override { 
                return (text_.size() > 0 && text_.at(0) == '\''
                        ? std::string(text_.begin() + 1, text_.end())
                        : text_);
            }
            std::string GetText() const override { return text_; }
            std::string GetInitialText() const override { return text_; }
        private: 
            std::string text_;
    };
    class FormulaImpl final : public Impl {
        public:
            FormulaImpl(std::string text, std::unique_ptr<FormulaInterface> formula, Sheet& sheet) :
                text_(text),
                formula_(std::move(formula)),
                sheet_(sheet) {
            }
            Value GetValue() const override {
                FormulaInterface::Value value = formula_->Evaluate(sheet_);
                if (std::holds_alternative<double>(value)) {
                    return std::get<double>(value);
                }
                return std::get<FormulaError>(value);
            }
            std::string GetText() const override {
                return '=' + formula_->GetExpression(); 
            }
            std::string GetInitialText() const override { 
                return text_; 
            }
            std::vector<Position> GetReferencedCells() const {
                return formula_->GetReferencedCells();
            }

        private: 
            std::string text_;
            std::unique_ptr<FormulaInterface> formula_;
            // таблица ячейки 
            // (необходима для получения доступа к ячейкам в случае формульных ячеек, содержащих в формулах индексы на ячейки)
            Sheet& sheet_;
    };

private:
    void ClearValueCache() {
        value_cache_ = std::nullopt;

        // Если были ячейки, которые зависят от текущей ячейки: сбрасываем их кэш значений тоже
        for (auto cell_from : cells_from_) {
            cell_from->ClearValueCache();
        }
    }

    bool CheckCircularDependency(const std::vector<Position>& referenced_cells) const {
        for (auto referenced_cell : referenced_cells) {
            std::vector<Position> checked_cells;
            if (CheckCircularDependency(referenced_cell, checked_cells)) {
                return true;
            }
        }
        return false;
    }

    bool CheckCircularDependency(Position cell_position, std::vector<Position>& checked_cells) const {
        if (!cell_position.IsValid()) {
            return false;
        }
        auto cell = sheet_.GetConcreteCell(cell_position);
        if (!cell) {
            return false;
        }
        if (cell == this) {
            return true;
        }
    
        // Помечаем ячейку, что мы её прошли
        checked_cells.push_back(cell_position);

        // Проходим по ячейкам, от которых зависит cell_position
        auto next_cells = cell->GetReferencedCells();
        for (auto next_cell : next_cells) {
            if (std::find(checked_cells.begin(), checked_cells.end(), next_cell) != checked_cells.end()) {
                return true;
            }

            // Продолжаем обход ячеек
            if (CheckCircularDependency(next_cell, checked_cells)) {
                return true;
            }
        }
        return false;
    }

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