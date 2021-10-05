# ads-query

`ads-query` is a simple utility for downloading single papers from nasa ads/arXiv from the command line. 

## Compilation

`ads-query` has two dependencies: `libcurl`, `libcjson`. With these install on the system compile the source with:
```bash
gcc -o ads-query ads-query.c -lcurl -lcjson
```

## How to use ads-query

The idea behind `ads-query` is to search for a variety of fields to locate the paper you want to download from NASAADS. This search is no where as friendly as using the normal ads search client, so should not be used to "browse" papers - instead, if you have come across a reference while reading another paper and want to quickly grab it, execute a simple search without loading up the site.

You will first need a NASA-ADS API token, see the below section on how to get one.

You can search on a number of fields included in the script, see `-h` or `-vh` for information on each. Additionally after all arguments are supplied, you can simply include more search terms of your own at the end of the command. These additional terms can be simple keywords, or ads search strings like `author:"NAME"`. 

If the search does not return a single paper, you will be presented with a list- to select from. The length of this list can be changed with `-r VALUE` option, it will be 10 by default. Select the paper with `1-10..` or `x` if you don't want any.

When searching for a keyword in the title `-T`, abstract `-A`, full text `-F`, keywords `-K`, if you search for more than one word, put quotation marks around the argument: `-A "Planetary Nebulae"`.

By default (at least for now), `ads-query` will download the arXiv version of the paper but if you desire the publication version give the `-P` option. Note however that some publication sites will take you to a html page, which the script will download accidentally.

If you wish to also export the bibtex file for the paper, give `-B`.


## API Token

To use this script you will need to have a developer token from NASAADS. This can be retrieved at: https://ui.adsabs.harvard.edu/user/settings/token
Once you have a token, this must supplied to the program using -tTOKEN or can be stored as the environment variable NASAADSTOKEN with:
```bash
export NASAADSTOKEN={TOKEN}` 
```

## Examples

```bash
#show help options
ads-query -vh

#simple search on keywords
ads-query -vo paper.pdf stars and galaxies

#wide search on few fields (then select from list)
ads-query -t{TOKEN} -aJones -pmnras -r25

#comprohensive search, downloading arXiv version, bibtex file and rename output
ads-query -vXB -t{TOKEN} -aJones -y2021 -pmnras -T"M32 Variables -o paper.pdf" 

```

## Caveats 

Renaming output files doesn't work all that well for now. If you export bibtex it will save to the current folder, overwriting any previous bibtex file that was there. 
