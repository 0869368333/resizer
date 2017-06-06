#!/bin/sh

# this should be set to the travis build dir
if [ "${TRAVIS_BUILD_DIR}" = "" ]; then
  exit 1
fi

# this should come from encrypted travis stuff
if [ "${GITHUB_TOKEN}" = "" ]; then
  exit 2
fi


# this should be set to the branch name
if [ "${TRAVIS_BRANCH}" = "" ]; then
  exit 3
fi

# TRAVIS_PULL_REQUEST should be unset
if [ ! "${TRAVIS_PULL_REQUEST}" = "" ]; then
  exit 4
fi

if [ "$(git config user.email)" = "" ]; then
  git config --global user.name "Imazen Bot"
  git config --global user.email "codebot@imazen.io"
fi

cd "${TRAVIS_BUILD_DIR}/Tools/DocCollector" || exit
git clone "https://${GITHUB_TOKEN}@github.com/imazen/resizer-web.git"
cd "${TRAVIS_BUILD_DIR}/Tools/DocCollector/resizer-web" || exit
git remote add pr "https://${GITHUB_TOKEN}@github.com/imazen-bot/resizer-web.git"
git checkout -f production
git pull
cd "${TRAVIS_BUILD_DIR}/Tools/DocCollector" || exit
bundle -j4
bundle exec rake "resizer[${TRAVIS_BUILD_DIR}]"
cd "${TRAVIS_BUILD_DIR}/Tools/DocCollector/resizer-web" || exit
git branch -D "${TRAVIS_BRANCH}_docs"
git checkout -b "${TRAVIS_BRANCH}_docs"
git add .
git status
git commit -m "DocCollector Update for imazen/resizer#${TRAVIS_PULL_REQUEST}"
# delete previous branch
git push pr :"${TRAVIS_BRANCH}_docs"
git push -u pr "${TRAVIS_BRANCH}_docs"

base="imazen/resizer-web:production"
head="imazen-bot/resizer-web:${TRAVIS_BRANCH}_docs_${TRAVIS_BUILD_NUMBER}"
hub="${TRAVIS_BUILD_DIR}/Tools/DocCollector/hub/hub-$(uname -s)-$(uname -m)-2.2.9"
$hub pull-request -m "DocCollector Update" -b "$base" -h "$head"