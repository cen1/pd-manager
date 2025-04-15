#!/bin/bash

# Set working directory

IP_COUNTRY_CSV_URL="https://cdn.jsdelivr.net/npm/@ip-location-db/geo-whois-asn-country/geo-whois-asn-country-ipv4-num.csv"
COUNTRY_CODES_CSV_URL="https://raw.githubusercontent.com/lukes/ISO-3166-Countries-with-Regional-Codes/master/all/all.csv"

# File paths
IP_COUNTRY_CSV="tmp_ip_country.csv"
COUNTRY_CODES_CSV="tmp_country_codes.csv"
MERGED_CSV="tmp_ip_country_merged.csv"

# MySQL credentials
MYSQL_USER="pdm"
MYSQL_PASS="changeme"
MYSQL_DB="pdm"
MYSQL_TABLE="new_ip_to_country"

# Download the IP-to-country CSV
curl -s -o "$IP_COUNTRY_CSV" "$IP_COUNTRY_CSV_URL"

# Download the country codes CSV
curl -s -o "$COUNTRY_CODES_CSV" "$COUNTRY_CODES_CSV_URL"

declare -A COUNTRY_CODE_MAP
declare -A COUNTRY_NAME_MAP

# Read the country codes CSV line by line
while IFS= read -r line; do
    # Skip the header line
    if [[ "$line" =~ ^name,alpha-2,alpha-3 ]]; then
        continue
    fi

    # Initialize variables
    cname=""
    alpha2=""
    alpha3=""

    # Parse the line manually to handle quoted country names
    # We check if the first field is quoted and split accordingly
    if [[ "$line" =~ ^\"(.*)\"\,([A-Za-z]{2})\,([A-Za-z]{3}) ]]; then
        cname="${BASH_REMATCH[1]}"  # Country name
        alpha2="${BASH_REMATCH[2]}" # Alpha-2 code
        alpha3="${BASH_REMATCH[3]}" # Alpha-3 code
    else
        # If the country name is not quoted, parse it normally
        IFS=',' read -r cname alpha2 alpha3 _ _ _ _ _ _ _ <<< "$line"
    fi

    # Remove quotes from country name, if present
    cname=${cname//\"/}

    # Update associative arrays
    COUNTRY_CODE_MAP[$alpha2]=$alpha3
    COUNTRY_NAME_MAP[$alpha2]=$cname
done < "$COUNTRY_CODES_CSV"

# Check the mappings
#for key in "${!COUNTRY_CODE_MAP[@]}"; do
#    echo "$key -> ${COUNTRY_CODE_MAP[$key]} -> ${COUNTRY_NAME_MAP[$key]}"
#done

# Process the IP-to-country CSV and create the merged CSV
{
    while IFS=, read -r start_ip end_ip country_code; do
        # Remove quotes if present
        country_code=${country_code//\"/}
        # Lookup the alpha-3 code and country name
        alpha3=${COUNTRY_CODE_MAP[$country_code]}
        country_name=${COUNTRY_NAME_MAP[$country_code]}
        # Output the merged line
        echo "\"$start_ip\",\"$end_ip\",\"$country_code\",\"$alpha3\",\"$country_name\""
    done < "$IP_COUNTRY_CSV"
} > "$MERGED_CSV"

head "$MERGED_CSV"

# Import the merged CSV into db
mysql --local-infile=1 -P 3306 -u"$MYSQL_USER" -p"$MYSQL_PASS" "$MYSQL_DB" <<EOF
LOAD DATA LOCAL INFILE '$MERGED_CSV'
INTO TABLE $MYSQL_TABLE
FIELDS TERMINATED BY ',' ENCLOSED BY '"'
LINES TERMINATED BY '\n'
(ip_from, ip_to, country_code2, country_code3, country_name);
EOF

rm "$IP_COUNTRY_CSV" "$COUNTRY_CODES_CSV" "$MERGED_CSV"

echo "IP to country data import complete."
