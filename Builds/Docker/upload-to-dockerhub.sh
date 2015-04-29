SHA1=$1
BRANCH=$2
if [ -n "$DOCKER_EMAIL" ]; then
  docker login -e $DOCKER_EMAIL -u $DOCKER_USERNAME -p $DOCKER_PASSWORD
  docker push ripple/rippled:$SHA1
  docker push ripple/rippled:$CIRCLE_BRANCH
  docker push ripple/rippled:latest
else
  echo "\$DOCKER_EMAIL not set. Not uploading to docker hub."
fi
