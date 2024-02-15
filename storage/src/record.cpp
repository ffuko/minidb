#include "../include/record.h"

namespace storage {
// TODO: better singleton pattern solution

const FieldMeta InfiFieldMeta = FieldMeta{
    .type = FieldType::Supremum, .name = "infimum", .is_primary = true};
const FieldMeta SupreFieldMeta = FieldMeta{
    .type = FieldType::Supremum, .name = "supremum", .is_primary = true};
const Key SupremumKey = Key(&InfiFieldMeta);

const RecordMeta InfiRecordMeta = RecordMeta{
    .type = RecordType::Infi,
};
const RecordMeta SupreRecordMeta = RecordMeta{
    .type = RecordType::Supre,
};

const InfiRecord Infimum = InfiRecord(&InfiRecordMeta, Key(&InfiFieldMeta));
const SupreRecord Supremum = SupreRecord(&SupreRecordMeta, Key(&InfiFieldMeta));
} // namespace storage
