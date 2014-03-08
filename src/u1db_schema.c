/**
 * Copyright 2012 Canonical Ltd.
 *
 * This file is part of u1db.
 *
 * This file was auto-generated using sql_to_c.py ['/home/kali/src/u1db-lp/trunk/u1db/backends/dbschema.sql', 'u1db__schema', '/home/kali/src/u1db-lp/trunk/src/u1db_schema.c']
 * Do not edit it directly.
 *
 * u1db is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3
 * as published by the Free Software Foundation.
 *
 * u1db is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with u1db.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

static const char *tmp[] = {
    "-- Database schema\n"
    "CREATE TABLE transaction_log (\n"
    "    generation INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    doc_id TEXT NOT NULL,\n"
    "    transaction_id TEXT NOT NULL\n"
    ")\n",

    "CREATE TABLE document (\n"
    "    doc_id TEXT PRIMARY KEY,\n"
    "    doc_rev TEXT NOT NULL,\n"
    "    content TEXT\n"
    ")\n",

    "CREATE TABLE document_fields (\n"
    "    doc_id TEXT NOT NULL,\n"
    "    field_name TEXT NOT NULL,\n"
    "    value TEXT\n"
    ")\n",

    "CREATE INDEX document_fields_field_value_doc_idx\n"
    "    ON document_fields(field_name, value, doc_id)\n",

    "CREATE TABLE sync_log (\n"
    "    replica_uid TEXT PRIMARY KEY,\n"
    "    known_generation INTEGER,\n"
    "    known_transaction_id TEXT\n"
    ")\n",

    "CREATE TABLE conflicts (\n"
    "    doc_id TEXT,\n"
    "    doc_rev TEXT,\n"
    "    content TEXT,\n"
    "    CONSTRAINT conflicts_pkey PRIMARY KEY (doc_id, doc_rev)\n"
    ")\n",

    "CREATE TABLE index_definitions (\n"
    "    name TEXT,\n"
    "    offset INT,\n"
    "    field TEXT,\n"
    "    CONSTRAINT index_definitions_pkey PRIMARY KEY (name, offset)\n"
    ")\n",

    "create index index_definitions_field on index_definitions(field)\n",

    "CREATE TABLE u1db_config (\n"
    "    name TEXT PRIMARY KEY,\n"
    "    value TEXT\n"
    ")\n",

    "INSERT INTO u1db_config VALUES ('sql_schema', '0')\n"
};
const char **u1db__schema = tmp;
const int u1db__schema_len = sizeof(tmp) / sizeof(char*);
